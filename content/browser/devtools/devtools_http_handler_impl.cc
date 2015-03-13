// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/system_info_handler.h"
#include "content/browser/devtools/protocol/tethering_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "grit/devtools_resources_map.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/server_socket.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

namespace {

const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

const char kDevToolsHandlerThreadName[] = "Chrome_DevToolsHandlerThread";

const char kThumbUrlPrefix[] = "/thumb/";
const char kPageUrlPrefix[] = "/devtools/page/";

const char kTargetIdField[] = "id";
const char kTargetParentIdField[] = "parentId";
const char kTargetTypeField[] = "type";
const char kTargetTitleField[] = "title";
const char kTargetDescriptionField[] = "description";
const char kTargetUrlField[] = "url";
const char kTargetThumbnailUrlField[] = "thumbnailUrl";
const char kTargetFaviconUrlField[] = "faviconUrl";
const char kTargetWebSocketDebuggerUrlField[] = "webSocketDebuggerUrl";
const char kTargetDevtoolsFrontendUrlField[] = "devtoolsFrontendUrl";

// Maximum write buffer size of devtools http/websocket connections.
// TODO(rmcilroy/pfieldman): Reduce this back to 100Mb when we have
// added back pressure on the TraceComplete message protocol - crbug.com/456845.
const int32 kSendBufferSizeForDevTools = 256 * 1024 * 1024;  // 256Mb

class BrowserTarget;
class DevToolsAgentHostClientImpl;
class ServerWrapper;

// DevToolsHttpHandlerImpl ---------------------------------------------------

class DevToolsHttpHandlerImpl : public DevToolsHttpHandler {
 public:
  DevToolsHttpHandlerImpl(scoped_ptr<ServerSocketFactory> server_socket_factory,
                          const std::string& frontend_url,
                          DevToolsHttpHandlerDelegate* delegate,
                          const base::FilePath& output_directory);
  ~DevToolsHttpHandlerImpl() override;

  void OnJsonRequest(int connection_id,
                     const net::HttpServerRequestInfo& info);
  void OnThumbnailRequest(int connection_id, const std::string& target_id);
  void OnDiscoveryPageRequest(int connection_id);

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info);
  void OnWebSocketMessage(int connection_id, const std::string& data);
  void OnClose(int connection_id);

  void ServerStarted(base::Thread* thread,
                     ServerWrapper* server_wrapper,
                     ServerSocketFactory* socket_factory,
                     scoped_ptr<net::IPEndPoint> ip_address);

 private:
  // DevToolsHttpHandler implementation.
  GURL GetFrontendURL(const std::string& path) override;

  static void OnTargetListReceivedWeak(
      base::WeakPtr<DevToolsHttpHandlerImpl> handler,
      int connection_id,
      const std::string& host,
      const DevToolsManagerDelegate::TargetList& targets);
  void OnTargetListReceived(
      int connection_id,
      const std::string& host,
      const DevToolsManagerDelegate::TargetList& targets);

  DevToolsTarget* GetTarget(const std::string& id);

  void SendJson(int connection_id,
                net::HttpStatusCode status_code,
                base::Value* value,
                const std::string& message);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);

  // Returns the front end url without the host at the beginning.
  std::string GetFrontendURLInternal(const std::string target_id,
                                     const std::string& host);

  base::DictionaryValue* SerializeTarget(const DevToolsTarget& target,
                                         const std::string& host);

  // The thread used by the devtools handler to run server socket.
  base::Thread* thread_;

  std::string frontend_url_;
  ServerWrapper* server_wrapper_;
  scoped_ptr<net::IPEndPoint> server_ip_address_;
  typedef std::map<int, DevToolsAgentHostClientImpl*> ConnectionToClientMap;
  ConnectionToClientMap connection_to_client_;
  const scoped_ptr<DevToolsHttpHandlerDelegate> delegate_;
  ServerSocketFactory* socket_factory_;
  typedef std::map<std::string, DevToolsTarget*> TargetMap;
  TargetMap target_map_;
  typedef std::map<int, BrowserTarget*> BrowserTargets;
  BrowserTargets browser_targets_;
  base::WeakPtrFactory<DevToolsHttpHandlerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandlerImpl);
};

// ServerWrapper -------------------------------------------------------------
// All methods in this class are only called on handler thread.
class ServerWrapper : net::HttpServer::Delegate {
 public:
  ServerWrapper(base::WeakPtr<DevToolsHttpHandlerImpl> handler,
                scoped_ptr<net::ServerSocket> socket,
                const base::FilePath& frontend_dir,
                bool bundles_resources);

  int GetLocalAddress(net::IPEndPoint* address);

  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);
  void SendOverWebSocket(int connection_id, const std::string& message);
  void SendResponse(int connection_id,
                    const net::HttpServerResponseInfo& response);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id, const std::string& message);
  void Close(int connection_id);

  void WriteActivePortToUserProfile(const base::FilePath& output_directory);

  virtual ~ServerWrapper() {}

 private:
  // net::HttpServer::Delegate implementation.
  void OnConnect(int connection_id) override {}
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id,
                          const std::string& data) override;
  void OnClose(int connection_id) override;

  base::WeakPtr<DevToolsHttpHandlerImpl> handler_;
  scoped_ptr<net::HttpServer> server_;
  base::FilePath frontend_dir_;
  bool bundles_resources_;
};

ServerWrapper::ServerWrapper(base::WeakPtr<DevToolsHttpHandlerImpl> handler,
                             scoped_ptr<net::ServerSocket> socket,
                             const base::FilePath& frontend_dir,
                             bool bundles_resources)
    : handler_(handler),
      server_(new net::HttpServer(socket.Pass(), this)),
      frontend_dir_(frontend_dir),
      bundles_resources_(bundles_resources) {
}

int ServerWrapper::GetLocalAddress(net::IPEndPoint* address) {
  return server_->GetLocalAddress(address);
}

void ServerWrapper::AcceptWebSocket(int connection_id,
                                    const net::HttpServerRequestInfo& request) {
  server_->SetSendBufferSize(connection_id, kSendBufferSizeForDevTools);
  server_->AcceptWebSocket(connection_id, request);
}

void ServerWrapper::SendOverWebSocket(int connection_id,
                                      const std::string& message) {
  server_->SendOverWebSocket(connection_id, message);
}

void ServerWrapper::SendResponse(int connection_id,
                                 const net::HttpServerResponseInfo& response) {
  server_->SendResponse(connection_id, response);
}

void ServerWrapper::Send200(int connection_id,
                            const std::string& data,
                            const std::string& mime_type) {
  server_->Send200(connection_id, data, mime_type);
}

void ServerWrapper::Send404(int connection_id) {
  server_->Send404(connection_id);
}

void ServerWrapper::Send500(int connection_id,
                            const std::string& message) {
  server_->Send500(connection_id, message);
}

void ServerWrapper::Close(int connection_id) {
  server_->Close(connection_id);
}

// Thread and ServerWrapper lifetime management ------------------------------

void TerminateOnUI(base::Thread* thread,
                   ServerWrapper* server_wrapper,
                   DevToolsHttpHandler::ServerSocketFactory* socket_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (server_wrapper) {
    DCHECK(thread);
    thread->message_loop()->DeleteSoon(FROM_HERE, server_wrapper);
  }
  if (socket_factory) {
    DCHECK(thread);
    thread->message_loop()->DeleteSoon(FROM_HERE, socket_factory);
  }
  if (thread) {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, thread);
  }
}

void ServerStartedOnUI(
    base::WeakPtr<DevToolsHttpHandlerImpl> handler,
    base::Thread* thread,
    ServerWrapper* server_wrapper,
    DevToolsHttpHandler::ServerSocketFactory* socket_factory,
    scoped_ptr<net::IPEndPoint> ip_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler && thread && server_wrapper) {
    handler->ServerStarted(thread, server_wrapper, socket_factory,
                           ip_address.Pass());
  } else {
    TerminateOnUI(thread, server_wrapper, socket_factory);
  }
}

void StartServerOnHandlerThread(
    base::WeakPtr<DevToolsHttpHandlerImpl> handler,
    base::Thread* thread,
    DevToolsHttpHandler::ServerSocketFactory* server_socket_factory,
    const base::FilePath& output_directory,
    const base::FilePath& frontend_dir,
    bool bundles_resources) {
  DCHECK_EQ(thread->message_loop(), base::MessageLoop::current());
  ServerWrapper* server_wrapper = nullptr;
  scoped_ptr<net::ServerSocket> server_socket =
      server_socket_factory->CreateForHttpServer();
  scoped_ptr<net::IPEndPoint> ip_address(new net::IPEndPoint);
  if (server_socket) {
    server_wrapper = new ServerWrapper(handler, server_socket.Pass(),
                                       frontend_dir, bundles_resources);
    if (!output_directory.empty())
      server_wrapper->WriteActivePortToUserProfile(output_directory);

    if (server_wrapper->GetLocalAddress(ip_address.get()) != net::OK)
      ip_address.reset();
  } else {
    ip_address.reset();
    LOG(ERROR) << "Cannot start http server for devtools. Stop devtools.";
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ServerStartedOnUI,
                 handler,
                 thread,
                 server_wrapper,
                 server_socket_factory,
                 base::Passed(&ip_address)));
}

void StartServerOnFile(
    base::WeakPtr<DevToolsHttpHandlerImpl> handler,
    DevToolsHttpHandler::ServerSocketFactory* server_socket_factory,
    const base::FilePath& output_directory,
    const base::FilePath& frontend_dir,
    bool bundles_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  scoped_ptr<base::Thread> thread(new base::Thread(kDevToolsHandlerThreadName));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (thread->StartWithOptions(options)) {
    base::MessageLoop* message_loop = thread->message_loop();
    message_loop->PostTask(FROM_HERE,
        base::Bind(&StartServerOnHandlerThread,
                   handler,
                   base::Unretained(thread.release()),
                   server_socket_factory,
                   output_directory,
                   frontend_dir,
                   bundles_resources));
  }
}

// DevToolsAgentHostClientImpl -----------------------------------------------
// An internal implementation of DevToolsAgentHostClient that delegates
// messages sent to a DebuggerShell instance.
class DevToolsAgentHostClientImpl : public DevToolsAgentHostClient {
 public:
  DevToolsAgentHostClientImpl(base::MessageLoop* message_loop,
                              ServerWrapper* server_wrapper,
                              int connection_id,
                              DevToolsAgentHost* agent_host)
      : message_loop_(message_loop),
        server_wrapper_(server_wrapper),
        connection_id_(connection_id),
        agent_host_(agent_host) {
    agent_host_->AttachClient(this);
  }

  ~DevToolsAgentHostClientImpl() override {
    if (agent_host_.get())
      agent_host_->DetachClient();
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override {
    DCHECK(agent_host == agent_host_.get());

    base::Callback<void(const std::string&)> raw_message_callback(
        base::Bind(&DevToolsAgentHostClientImpl::DispatchProtocolMessage,
                   base::Unretained(this), base::Unretained(agent_host)));
    devtools::inspector::Client inspector(raw_message_callback);
    inspector.Detached(devtools::inspector::DetachedParams::Create()
       ->set_reason(replaced_with_another_client ?
            "replaced_with_devtools" : "target_closed"));

    agent_host_ = nullptr;
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ServerWrapper::Close,
                   base::Unretained(server_wrapper_),
                   connection_id_));
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    DCHECK(agent_host == agent_host_.get());
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ServerWrapper::SendOverWebSocket,
                   base::Unretained(server_wrapper_),
                   connection_id_,
                   message));
  }

  void OnMessage(const std::string& message) {
    if (agent_host_.get())
      agent_host_->DispatchProtocolMessage(message);
  }

 private:
  base::MessageLoop* const message_loop_;
  ServerWrapper* const server_wrapper_;
  const int connection_id_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

static bool TimeComparator(const DevToolsTarget* target1,
                           const DevToolsTarget* target2) {
  return target1->GetLastActivityTime() > target2->GetLastActivityTime();
}

// BrowserTarget -------------------------------------------------------------

class BrowserTarget {
 public:
  BrowserTarget(base::MessageLoop* message_loop,
                ServerWrapper* server_wrapper,
                DevToolsHttpHandler::ServerSocketFactory* socket_factory,
                int connection_id)
      : message_loop_(message_loop),
        server_wrapper_(server_wrapper),
        connection_id_(connection_id),
        system_info_handler_(new devtools::system_info::SystemInfoHandler()),
        tethering_handler_(new devtools::tethering::TetheringHandler(
            socket_factory, message_loop->message_loop_proxy())),
        tracing_handler_(new devtools::tracing::TracingHandler(
            devtools::tracing::TracingHandler::Browser)),
        protocol_handler_(new DevToolsProtocolHandler(
            base::Bind(&BrowserTarget::Respond, base::Unretained(this)))) {
    DevToolsProtocolDispatcher* dispatcher = protocol_handler_->dispatcher();
    dispatcher->SetSystemInfoHandler(system_info_handler_.get());
    dispatcher->SetTetheringHandler(tethering_handler_.get());
    dispatcher->SetTracingHandler(tracing_handler_.get());
  }

  void HandleMessage(const std::string& message) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::string error_response;
    scoped_ptr<base::DictionaryValue> command =
        protocol_handler_->ParseCommand(message);
    if (command)
      protocol_handler_->HandleCommand(command.Pass());
  }

  void Respond(const std::string& message) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ServerWrapper::SendOverWebSocket,
                   base::Unretained(server_wrapper_),
                   connection_id_,
                   message));
  }

 private:
  base::MessageLoop* const message_loop_;
  ServerWrapper* const server_wrapper_;
  const int connection_id_;
  scoped_ptr<devtools::system_info::SystemInfoHandler> system_info_handler_;
  scoped_ptr<devtools::tethering::TetheringHandler> tethering_handler_;
  scoped_ptr<devtools::tracing::TracingHandler> tracing_handler_;
  scoped_ptr<DevToolsProtocolHandler> protocol_handler_;
};

}  // namespace

// DevToolsHttpHandler -------------------------------------------------------

// static
bool DevToolsHttpHandler::IsSupportedProtocolVersion(
    const std::string& version) {
  return devtools::IsSupportedProtocolVersion(version);
}

// static
int DevToolsHttpHandler::GetFrontendResourceId(const std::string& name) {
  for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
    if (name == kDevtoolsResources[i].name)
      return kDevtoolsResources[i].value;
  }
  return -1;
}

// static
DevToolsHttpHandler* DevToolsHttpHandler::Start(
    scoped_ptr<ServerSocketFactory> server_socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate,
    const base::FilePath& active_port_output_directory) {
  DevToolsHttpHandlerImpl* http_handler =
      new DevToolsHttpHandlerImpl(server_socket_factory.Pass(),
                                  frontend_url,
                                  delegate,
                                  active_port_output_directory);
  return http_handler;
}

// DevToolsHttpHandler::ServerSocketFactory ----------------------------------

scoped_ptr<net::ServerSocket>
DevToolsHttpHandler::ServerSocketFactory::CreateForHttpServer() {
  return scoped_ptr<net::ServerSocket>();
}

scoped_ptr<net::ServerSocket>
DevToolsHttpHandler::ServerSocketFactory::CreateForTethering(
    std::string* name) {
  return scoped_ptr<net::ServerSocket>();
}

// DevToolsHttpHandlerImpl ---------------------------------------------------

DevToolsHttpHandlerImpl::~DevToolsHttpHandlerImpl() {
  TerminateOnUI(thread_, server_wrapper_, socket_factory_);
  STLDeleteValues(&target_map_);
  STLDeleteValues(&browser_targets_);
  STLDeleteValues(&connection_to_client_);
}

GURL DevToolsHttpHandlerImpl::GetFrontendURL(const std::string& path) {
  if (!server_ip_address_)
    return GURL();
  return GURL(std::string("http://") + server_ip_address_->ToString() +
              (path.empty() ? frontend_url_ : path));
}

static std::string PathWithoutParams(const std::string& path) {
  size_t query_position = path.find("?");
  if (query_position != std::string::npos)
    return path.substr(0, query_position);
  return path;
}

static std::string GetMimeType(const std::string& filename) {
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  } else if (EndsWith(filename, ".json", false)) {
    return "application/json";
  }
  LOG(ERROR) << "GetMimeType doesn't know mime type for: "
             << filename
             << " text/plain will be returned";
  NOTREACHED();
  return "text/plain";
}

void ServerWrapper::OnHttpRequest(int connection_id,
                                  const net::HttpServerRequestInfo& info) {
  server_->SetSendBufferSize(connection_id, kSendBufferSizeForDevTools);

  if (info.path.find("/json") == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnJsonRequest,
                   handler_,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find(kThumbUrlPrefix) == 0) {
    // Thumbnail request.
    const std::string target_id = info.path.substr(strlen(kThumbUrlPrefix));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnThumbnailRequest,
                   handler_,
                   connection_id,
                   target_id));
    return;
  }

  if (info.path.empty() || info.path == "/") {
    // Discovery page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnDiscoveryPageRequest,
                   handler_,
                   connection_id));
    return;
  }

  if (info.path.find("/devtools/") != 0) {
    server_->Send404(connection_id);
    return;
  }

  std::string filename = PathWithoutParams(info.path.substr(10));
  std::string mime_type = GetMimeType(filename);

  if (!frontend_dir_.empty()) {
    base::FilePath path = frontend_dir_.AppendASCII(filename);
    std::string data;
    base::ReadFileToString(path, &data);
    server_->Send200(connection_id, data, mime_type);
    return;
  }
  if (bundles_resources_) {
    int resource_id = DevToolsHttpHandler::GetFrontendResourceId(filename);
    if (resource_id != -1) {
      base::StringPiece data = GetContentClient()->GetDataResource(
          resource_id, ui::SCALE_FACTOR_NONE);
      server_->Send200(connection_id, data.as_string(), mime_type);
      return;
    }
  }
  server_->Send404(connection_id);
}

void ServerWrapper::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketRequest,
          handler_,
          connection_id,
          request));
}

void ServerWrapper::OnWebSocketMessage(int connection_id,
                                       const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketMessage,
          handler_,
          connection_id,
          data));
}

void ServerWrapper::OnClose(int connection_id) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnClose,
          handler_,
          connection_id));
}

std::string DevToolsHttpHandlerImpl::GetFrontendURLInternal(
    const std::string id,
    const std::string& host) {
  return base::StringPrintf(
      "%s%sws=%s%s%s",
      frontend_url_.c_str(),
      frontend_url_.find("?") == std::string::npos ? "?" : "&",
      host.c_str(),
      kPageUrlPrefix,
      id.c_str());
}

static bool ParseJsonPath(
    const std::string& path,
    std::string* command,
    std::string* target_id) {

  // Fall back to list in case of empty query.
  if (path.empty()) {
    *command = "list";
    return true;
  }

  if (path.find("/") != 0) {
    // Malformed command.
    return false;
  }
  *command = path.substr(1);

  size_t separator_pos = command->find("/");
  if (separator_pos != std::string::npos) {
    *target_id = command->substr(separator_pos + 1);
    *command = command->substr(0, separator_pos);
  }
  return true;
}

void DevToolsHttpHandlerImpl::OnJsonRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // Trim /json
  std::string path = info.path.substr(5);

  // Trim fragment and query
  std::string query;
  size_t query_pos = path.find("?");
  if (query_pos != std::string::npos) {
    query = path.substr(query_pos + 1);
    path = path.substr(0, query_pos);
  }

  size_t fragment_pos = path.find("#");
  if (fragment_pos != std::string::npos)
    path = path.substr(0, fragment_pos);

  std::string command;
  std::string target_id;
  if (!ParseJsonPath(path, &command, &target_id)) {
    SendJson(connection_id,
             net::HTTP_NOT_FOUND,
             NULL,
             "Malformed query: " + info.path);
    return;
  }

  if (command == "version") {
    base::DictionaryValue version;
    version.SetString("Protocol-Version", devtools::kProtocolVersion);
    version.SetString("WebKit-Version", GetWebKitVersion());
    version.SetString("Browser", GetContentClient()->GetProduct());
    version.SetString("User-Agent", GetContentClient()->GetUserAgent());
#if defined(OS_ANDROID)
    version.SetString("Android-Package",
        base::android::BuildInfo::GetInstance()->package_name());
#endif
    SendJson(connection_id, net::HTTP_OK, &version, std::string());
    return;
  }

  if (command == "list") {
    std::string host = info.headers["host"];
    DevToolsManagerDelegate* manager_delegate =
        DevToolsManager::GetInstance()->delegate();
    if (manager_delegate) {
      manager_delegate->EnumerateTargets(
          base::Bind(&DevToolsHttpHandlerImpl::OnTargetListReceivedWeak,
                     weak_factory_.GetWeakPtr(), connection_id, host));
    } else {
      DevToolsManagerDelegate::TargetList empty_list;
      OnTargetListReceived(connection_id, host, empty_list);
    }
    return;
  }

  if (command == "new") {
    GURL url(net::UnescapeURLComponent(
        query, net::UnescapeRule::URL_SPECIAL_CHARS));
    if (!url.is_valid())
      url = GURL(url::kAboutBlankURL);
    scoped_ptr<DevToolsTarget> target;
    DevToolsManagerDelegate* manager_delegate =
        DevToolsManager::GetInstance()->delegate();
    if (manager_delegate)
      target = manager_delegate->CreateNewTarget(url);
    if (!target) {
      SendJson(connection_id,
               net::HTTP_INTERNAL_SERVER_ERROR,
               NULL,
               "Could not create new page");
      return;
    }
    std::string host = info.headers["host"];
    scoped_ptr<base::DictionaryValue> dictionary(
        SerializeTarget(*target.get(), host));
    SendJson(connection_id, net::HTTP_OK, dictionary.get(), std::string());
    const std::string target_id = target->GetId();
    target_map_[target_id] = target.release();
    return;
  }

  if (command == "activate" || command == "close") {
    DevToolsTarget* target = GetTarget(target_id);
    if (!target) {
      SendJson(connection_id,
               net::HTTP_NOT_FOUND,
               NULL,
               "No such target id: " + target_id);
      return;
    }

    if (command == "activate") {
      if (target->Activate()) {
        SendJson(connection_id, net::HTTP_OK, NULL, "Target activated");
      } else {
        SendJson(connection_id,
                 net::HTTP_INTERNAL_SERVER_ERROR,
                 NULL,
                 "Could not activate target id: " + target_id);
      }
      return;
    }

    if (command == "close") {
      if (target->Close()) {
        SendJson(connection_id, net::HTTP_OK, NULL, "Target is closing");
      } else {
        SendJson(connection_id,
                 net::HTTP_INTERNAL_SERVER_ERROR,
                 NULL,
                 "Could not close target id: " + target_id);
      }
      return;
    }
  }
  SendJson(connection_id,
           net::HTTP_NOT_FOUND,
           NULL,
           "Unknown command: " + command);
  return;
}

// static
void DevToolsHttpHandlerImpl::OnTargetListReceivedWeak(
    base::WeakPtr<DevToolsHttpHandlerImpl> handler,
    int connection_id,
    const std::string& host,
    const DevToolsManagerDelegate::TargetList& targets) {
  if (handler) {
    handler->OnTargetListReceived(connection_id, host, targets);
  } else {
    STLDeleteContainerPointers(targets.begin(), targets.end());
  }
}

void DevToolsHttpHandlerImpl::OnTargetListReceived(
    int connection_id,
    const std::string& host,
    const DevToolsManagerDelegate::TargetList& targets) {
  DevToolsManagerDelegate::TargetList sorted_targets = targets;
  std::sort(sorted_targets.begin(), sorted_targets.end(), TimeComparator);

  STLDeleteValues(&target_map_);
  base::ListValue list_value;
  for (DevToolsManagerDelegate::TargetList::const_iterator it =
      sorted_targets.begin(); it != sorted_targets.end(); ++it) {
    DevToolsTarget* target = *it;
    target_map_[target->GetId()] = target;
    list_value.Append(SerializeTarget(*target, host));
  }
  SendJson(connection_id, net::HTTP_OK, &list_value, std::string());
}

DevToolsTarget* DevToolsHttpHandlerImpl::GetTarget(const std::string& id) {
  TargetMap::const_iterator it = target_map_.find(id);
  if (it == target_map_.end())
    return NULL;
  return it->second;
}

void DevToolsHttpHandlerImpl::OnThumbnailRequest(
    int connection_id, const std::string& target_id) {
  DevToolsTarget* target = GetTarget(target_id);
  GURL page_url;
  if (target)
    page_url = target->GetURL();
  DevToolsManagerDelegate* manager_delegate =
      DevToolsManager::GetInstance()->delegate();
  std::string data =
      manager_delegate ? manager_delegate->GetPageThumbnailData(page_url) : "";
  if (!data.empty())
    Send200(connection_id, data, "image/png");
  else
    Send404(connection_id);
}

void DevToolsHttpHandlerImpl::OnDiscoveryPageRequest(int connection_id) {
  std::string response = delegate_->GetDiscoveryPageHTML();
  Send200(connection_id, response, "text/html; charset=UTF-8");
}

void DevToolsHttpHandlerImpl::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_)
    return;

  std::string browser_prefix = "/devtools/browser";
  size_t browser_pos = request.path.find(browser_prefix);
  if (browser_pos == 0) {
    browser_targets_[connection_id] = new BrowserTarget(thread_->message_loop(),
                                                        server_wrapper_,
                                                        socket_factory_,
                                                        connection_id);
    AcceptWebSocket(connection_id, request);
    return;
  }

  size_t pos = request.path.find(kPageUrlPrefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_id = request.path.substr(strlen(kPageUrlPrefix));
  DevToolsTarget* target = GetTarget(page_id);
  scoped_refptr<DevToolsAgentHost> agent =
      target ? target->GetAgentHost() : NULL;
  if (!agent.get()) {
    Send500(connection_id, "No such target id: " + page_id);
    return;
  }

  if (agent->IsAttached()) {
    Send500(connection_id,
            "Target with given id is being inspected: " + page_id);
    return;
  }

  DevToolsAgentHostClientImpl* client_host = new DevToolsAgentHostClientImpl(
      thread_->message_loop(), server_wrapper_, connection_id, agent.get());
  connection_to_client_[connection_id] = client_host;

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpHandlerImpl::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  BrowserTargets::iterator browser_it = browser_targets_.find(connection_id);
  if (browser_it != browser_targets_.end()) {
    browser_it->second->HandleMessage(data);
    return;
  }

  ConnectionToClientMap::iterator it =
      connection_to_client_.find(connection_id);
  if (it != connection_to_client_.end())
    it->second->OnMessage(data);
}

void DevToolsHttpHandlerImpl::OnClose(int connection_id) {
  BrowserTargets::iterator browser_it = browser_targets_.find(connection_id);
  if (browser_it != browser_targets_.end()) {
    delete browser_it->second;
    browser_targets_.erase(connection_id);
    return;
  }

  ConnectionToClientMap::iterator it =
      connection_to_client_.find(connection_id);
  if (it != connection_to_client_.end()) {
    delete it->second;
    connection_to_client_.erase(connection_id);
  }
}

DevToolsHttpHandlerImpl::DevToolsHttpHandlerImpl(
    scoped_ptr<ServerSocketFactory> server_socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate,
    const base::FilePath& output_directory)
    : thread_(nullptr),
      frontend_url_(frontend_url),
      server_wrapper_(nullptr),
      delegate_(delegate),
      socket_factory_(nullptr),
      weak_factory_(this) {
  if (frontend_url_.empty())
    frontend_url_ = "/devtools/inspector.html";

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&StartServerOnFile,
                 weak_factory_.GetWeakPtr(),
                 server_socket_factory.release(),
                 output_directory,
                 delegate_->GetDebugFrontendDir(),
                 delegate_->BundlesFrontendResources()));
}

void DevToolsHttpHandlerImpl::ServerStarted(
    base::Thread* thread,
    ServerWrapper* server_wrapper,
    ServerSocketFactory* socket_factory,
    scoped_ptr<net::IPEndPoint> ip_address) {
  thread_ = thread;
  server_wrapper_ = server_wrapper;
  socket_factory_ = socket_factory;
  server_ip_address_.swap(ip_address);
}

void ServerWrapper::WriteActivePortToUserProfile(
    const base::FilePath& output_directory) {
  DCHECK(!output_directory.empty());
  net::IPEndPoint endpoint;
  int err;
  if ((err = server_->GetLocalAddress(&endpoint)) != net::OK) {
    LOG(ERROR) << "Error " << err << " getting local address";
    return;
  }

  // Write this port to a well-known file in the profile directory
  // so Telemetry can pick it up.
  base::FilePath path = output_directory.Append(kDevToolsActivePortFileName);
  std::string port_string = base::IntToString(endpoint.port());
  if (base::WriteFile(path, port_string.c_str(), port_string.length()) < 0) {
    LOG(ERROR) << "Error writing DevTools active port to file";
  }
}

void DevToolsHttpHandlerImpl::SendJson(int connection_id,
                                       net::HttpStatusCode status_code,
                                       base::Value* value,
                                       const std::string& message) {
  if (!thread_)
    return;

  // Serialize value and message.
  std::string json_value;
  if (value) {
    base::JSONWriter::WriteWithOptions(value,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &json_value);
  }
  std::string json_message;
  scoped_ptr<base::Value> message_object(new base::StringValue(message));
  base::JSONWriter::Write(message_object.get(), &json_message);

  net::HttpServerResponseInfo response(status_code);
  response.SetBody(json_value + message, "application/json; charset=UTF-8");

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ServerWrapper::SendResponse,
                 base::Unretained(server_wrapper_),
                 connection_id,
                 response));
}

void DevToolsHttpHandlerImpl::Send200(int connection_id,
                                      const std::string& data,
                                      const std::string& mime_type) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ServerWrapper::Send200,
                 base::Unretained(server_wrapper_),
                 connection_id,
                 data,
                 mime_type));
}

void DevToolsHttpHandlerImpl::Send404(int connection_id) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ServerWrapper::Send404,
                 base::Unretained(server_wrapper_),
                 connection_id));
}

void DevToolsHttpHandlerImpl::Send500(int connection_id,
                                      const std::string& message) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ServerWrapper::Send500,
                 base::Unretained(server_wrapper_),
                 connection_id,
                 message));
}

void DevToolsHttpHandlerImpl::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ServerWrapper::AcceptWebSocket,
                 base::Unretained(server_wrapper_),
                 connection_id,
                 request));
}

base::DictionaryValue* DevToolsHttpHandlerImpl::SerializeTarget(
    const DevToolsTarget& target,
    const std::string& host) {
  base::DictionaryValue* dictionary = new base::DictionaryValue;

  std::string id = target.GetId();
  dictionary->SetString(kTargetIdField, id);
  std::string parent_id = target.GetParentId();
  if (!parent_id.empty())
    dictionary->SetString(kTargetParentIdField, parent_id);
  dictionary->SetString(kTargetTypeField, target.GetType());
  dictionary->SetString(kTargetTitleField,
                        net::EscapeForHTML(target.GetTitle()));
  dictionary->SetString(kTargetDescriptionField, target.GetDescription());

  GURL url = target.GetURL();
  dictionary->SetString(kTargetUrlField, url.spec());

  GURL favicon_url = target.GetFaviconURL();
  if (favicon_url.is_valid())
    dictionary->SetString(kTargetFaviconUrlField, favicon_url.spec());

  DevToolsManagerDelegate* manager_delegate =
      DevToolsManager::GetInstance()->delegate();
  if (manager_delegate &&
      !manager_delegate->GetPageThumbnailData(url).empty()) {
    dictionary->SetString(kTargetThumbnailUrlField,
                          std::string(kThumbUrlPrefix) + id);
  }

  if (!target.IsAttached()) {
    dictionary->SetString(kTargetWebSocketDebuggerUrlField,
                          base::StringPrintf("ws://%s%s%s",
                                             host.c_str(),
                                             kPageUrlPrefix,
                                             id.c_str()));
    std::string devtools_frontend_url = GetFrontendURLInternal(
        id.c_str(),
        host);
    dictionary->SetString(
        kTargetDevtoolsFrontendUrlField, devtools_frontend_url);
  }

  return dictionary;
}

}  // namespace content
