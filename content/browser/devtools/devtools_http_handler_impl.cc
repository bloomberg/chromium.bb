// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_http_handler_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_browser_target.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/devtools_resources_map.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server_request_info.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "ui/base/layout.h"
#include "webkit/user_agent/user_agent.h"
#include "webkit/user_agent/user_agent_util.h"

namespace content {

const int kBufferSize = 16 * 1024;

namespace {

static const char* kDevToolsHandlerThreadName = "Chrome_DevToolsHandlerThread";

static const char* kThumbUrlPrefix = "/thumb/";
static const char* kPageUrlPrefix = "/devtools/page/";

static const char* kTargetIdField = "id";
static const char* kTargetTypeField = "type";
static const char* kTargetTitleField = "title";
static const char* kTargetDescriptionField = "description";
static const char* kTargetUrlField = "url";
static const char* kTargetThumbnailUrlField = "thumbnailUrl";
static const char* kTargetFaviconUrlField = "faviconUrl";
static const char* kTargetWebSocketDebuggerUrlField = "webSocketDebuggerUrl";
static const char* kTargetDevtoolsFrontendUrlField = "devtoolsFrontendUrl";

static const char* kTargetTypePage = "page";
static const char* kTargetTypeOther = "other";

class DevToolsDefaultBindingHandler
    : public DevToolsHttpHandler::DevToolsAgentHostBinding {
 public:
  DevToolsDefaultBindingHandler() {
  }

  virtual std::string GetIdentifier(DevToolsAgentHost* agent_host) OVERRIDE {
    return agent_host->GetId();
  }

  virtual DevToolsAgentHost* ForIdentifier(const std::string& id) OVERRIDE {
    return DevToolsAgentHost::GetForId(id);
  }
};

// An internal implementation of DevToolsClientHost that delegates
// messages sent for DevToolsClient to a DebuggerShell instance.
class DevToolsClientHostImpl : public DevToolsClientHost {
 public:
  DevToolsClientHostImpl(
      MessageLoop* message_loop,
      net::HttpServer* server,
      int connection_id)
      : message_loop_(message_loop),
        server_(server),
        connection_id_(connection_id),
        is_closed_(false),
        detach_reason_("target_closed") {
  }

  virtual ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedContentsClosing() OVERRIDE {
    if (is_closed_)
      return;
    is_closed_ = true;

    std::string response =
        WebKit::WebDevToolsAgent::inspectorDetachedEvent(
            WebKit::WebString::fromUTF8(detach_reason_)).utf8();
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   response));

    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::Close, server_, connection_id_));
  }

  virtual void DispatchOnInspectorFrontend(const std::string& data) OVERRIDE {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   data));
  }

  virtual void ReplacedWithAnotherClient() OVERRIDE {
    detach_reason_ = "replaced_with_devtools";
  }

 private:
  MessageLoop* message_loop_;
  net::HttpServer* server_;
  int connection_id_;
  bool is_closed_;
  std::string detach_reason_;
};

static base::TimeTicks GetLastSelectedTime(RenderViewHost* rvh) {
  WebContents* web_contents = rvh->GetDelegate()->GetAsWebContents();
  if (!web_contents)
    return base::TimeTicks();

  return web_contents->GetLastSelectedTime();
}

typedef std::pair<RenderViewHost*, base::TimeTicks> PageInfo;

static bool TimeComparator(const PageInfo& info1, const PageInfo& info2) {
  return info1.second > info2.second;
}

}  // namespace

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
    const net::StreamListenSocketFactory* socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate) {
  DevToolsHttpHandlerImpl* http_handler =
      new DevToolsHttpHandlerImpl(socket_factory,
                                  frontend_url,
                                  delegate);
  http_handler->Start();
  return http_handler;
}

DevToolsHttpHandlerImpl::~DevToolsHttpHandlerImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Stop() must be called prior to destruction.
  DCHECK(server_.get() == NULL);
  DCHECK(thread_.get() == NULL);
}

void DevToolsHttpHandlerImpl::Start() {
  if (thread_.get())
    return;
  thread_.reset(new base::Thread(kDevToolsHandlerThreadName));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::StartHandlerThread, this));
}

// Runs on FILE thread.
void DevToolsHttpHandlerImpl::StartHandlerThread() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::ResetHandlerThread, this));
    return;
  }

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Init, this));
}

void DevToolsHttpHandlerImpl::ResetHandlerThread() {
  thread_.reset();
}

void DevToolsHttpHandlerImpl::ResetHandlerThreadAndRelease() {
  ResetHandlerThread();
  Release();
}

void DevToolsHttpHandlerImpl::Stop() {
  if (!thread_.get())
    return;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::StopHandlerThread, this),
      base::Bind(&DevToolsHttpHandlerImpl::ResetHandlerThreadAndRelease, this));
}

void DevToolsHttpHandlerImpl::SetDevToolsAgentHostBinding(
    DevToolsAgentHostBinding* binding) {
  if (binding)
    binding_ = binding;
  else
    binding_ = default_binding_.get();
}

GURL DevToolsHttpHandlerImpl::GetFrontendURL(DevToolsAgentHost* agent_host) {
  net::IPEndPoint ip_address;
  if (server_->GetLocalAddress(&ip_address))
    return GURL();
  if (!agent_host) {
    return GURL(std::string("http://") + ip_address.ToString() +
        overridden_frontend_url_);
  }
  std::string host = ip_address.ToString();
  std::string id = binding_->GetIdentifier(agent_host);
  return GURL(std::string("http://") +
              ip_address.ToString() +
              GetFrontendURLInternal(id, host));
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
  }
  NOTREACHED();
  return "text/plain";
}

void DevToolsHttpHandlerImpl::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  if (info.path.find("/json") == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnJsonRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find(kThumbUrlPrefix) == 0) {
    // Thumbnail request.
    const std::string target_id = info.path.substr(strlen(kThumbUrlPrefix));
    DevToolsAgentHost* agent_host = binding_->ForIdentifier(target_id);
    GURL page_url;
    if (agent_host) {
      RenderViewHost* rvh = agent_host->GetRenderViewHost();
      if (rvh)
        page_url = rvh->GetDelegate()->GetURL();
    }
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnThumbnailRequestUI,
                   this,
                   connection_id,
                   page_url));
    return;
  }

  if (info.path == "" || info.path == "/") {
    // Discovery page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnDiscoveryPageRequestUI,
                   this,
                   connection_id));
    return;
  }

  if (info.path.find("/devtools/") != 0) {
    server_->Send404(connection_id);
    return;
  }

  std::string filename = PathWithoutParams(info.path.substr(10));
  std::string mime_type = GetMimeType(filename);

  base::FilePath frontend_dir = delegate_->GetDebugFrontendDir();
  if (!frontend_dir.empty()) {
    base::FilePath path = frontend_dir.AppendASCII(filename);
    std::string data;
    file_util::ReadFileToString(path, &data);
    server_->Send200(connection_id, data, mime_type);
    return;
  }
  if (delegate_->BundlesFrontendResources()) {
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

void DevToolsHttpHandlerImpl::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketRequestUI,
          this,
          connection_id,
          request));
}

void DevToolsHttpHandlerImpl::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketMessageUI,
          this,
          connection_id,
          data));
}

void DevToolsHttpHandlerImpl::OnClose(int connection_id) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnCloseUI,
          this,
          connection_id));
}

std::string DevToolsHttpHandlerImpl::GetFrontendURLInternal(
    const std::string id,
    const std::string& host) {
  return base::StringPrintf(
      "%s%sws=%s%s%s",
      overridden_frontend_url_.c_str(),
      overridden_frontend_url_.find("?") == std::string::npos ? "?" : "&",
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

void DevToolsHttpHandlerImpl::OnJsonRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // Trim /json
  std::string path = info.path.substr(5);

  // Trim fragment and query
  size_t query_pos = path.find("?");
  if (query_pos != std::string::npos)
    path = path.substr(0, query_pos);

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
    version.SetString("Protocol-Version",
                      WebKit::WebDevToolsAgent::inspectorProtocolVersion());
    version.SetString("WebKit-Version",
                      webkit_glue::GetWebKitVersion());
    version.SetString("Browser",
                      content::GetContentClient()->GetProduct());
    version.SetString("User-Agent",
                      webkit_glue::GetUserAgent(GURL(chrome::kAboutBlankURL)));
    SendJson(connection_id, net::HTTP_OK, &version, "");
    return;
  }

  if (command == "list") {
    typedef std::vector<PageInfo> PageList;
    PageList page_list;

    std::vector<RenderViewHost*> rvh_list =
        DevToolsAgentHost::GetValidRenderViewHosts();
    for (std::vector<RenderViewHost*>::iterator it = rvh_list.begin();
         it != rvh_list.end(); ++it)
      page_list.push_back(PageInfo(*it, GetLastSelectedTime(*it)));

    std::sort(page_list.begin(), page_list.end(), TimeComparator);

    base::ListValue* target_list = new base::ListValue();
    std::string host = info.headers["Host"];
    for (PageList::iterator i = page_list.begin(); i != page_list.end(); ++i)
      target_list->Append(SerializePageInfo(i->first, host));

    AddRef();  // Balanced in SendTargetList.
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::CollectWorkerInfo,
                   base::Unretained(this),
                   target_list,
                   host),
        base::Bind(&DevToolsHttpHandlerImpl::SendTargetList,
                   base::Unretained(this),
                   connection_id,
                   target_list));
    return;
  }

  if (command == "new") {
    RenderViewHost* rvh = delegate_->CreateNewTarget();
    if (!rvh) {
      SendJson(connection_id,
               net::HTTP_INTERNAL_SERVER_ERROR,
               NULL,
               "Could not create new page");
      return;
    }
    std::string host = info.headers["Host"];
    scoped_ptr<base::DictionaryValue> dictionary(SerializePageInfo(rvh, host));
    SendJson(connection_id, net::HTTP_OK, dictionary.get(), "");
    return;
  }

  if (command == "activate" || command == "close") {
    DevToolsAgentHost* agent_host = binding_->ForIdentifier(target_id);
    RenderViewHost* rvh = agent_host ? agent_host->GetRenderViewHost() : NULL;
    if (!rvh) {
      SendJson(connection_id,
               net::HTTP_NOT_FOUND,
               NULL,
               "No such target id: " + target_id);
      return;
    }

    if (command == "activate") {
      rvh->GetDelegate()->Activate();
      SendJson(connection_id, net::HTTP_OK, NULL, "Target activated");
      return;
    }

    if (command == "close") {
      rvh->ClosePage();
      SendJson(connection_id, net::HTTP_OK, NULL, "Target is closing");
      return;
    }
  }
  SendJson(connection_id,
           net::HTTP_NOT_FOUND,
           NULL,
           "Unknown command: " + command);
  return;
}

void DevToolsHttpHandlerImpl::CollectWorkerInfo(ListValue* target_list,
                                                std::string host) {

  std::vector<WorkerService::WorkerInfo> worker_info =
      WorkerService::GetInstance()->GetWorkers();

  for (size_t i = 0; i < worker_info.size(); ++i)
    target_list->Append(SerializeWorkerInfo(worker_info[i], host));
}

void DevToolsHttpHandlerImpl::SendTargetList(int connection_id,
                                             ListValue* target_list) {
  SendJson(connection_id, net::HTTP_OK, target_list, "");
  delete target_list;
  Release();  // Balanced OnJsonRequestUI.
}

void DevToolsHttpHandlerImpl::OnThumbnailRequestUI(
    int connection_id, const GURL& page_url) {
  std::string data = delegate_->GetPageThumbnailData(page_url);
  if (!data.empty())
    Send200(connection_id, data, "image/png");
  else
    Send404(connection_id);
}

void DevToolsHttpHandlerImpl::OnDiscoveryPageRequestUI(int connection_id) {
  std::string response = delegate_->GetDiscoveryPageHTML();
  Send200(connection_id, response, "text/html; charset=UTF-8");
}

void DevToolsHttpHandlerImpl::OnWebSocketRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_.get())
    return;
  std::string browser_prefix = "/devtools/browser";
  size_t browser_pos = request.path.find(browser_prefix);
  if (browser_pos == 0) {
    if (browser_target_) {
      Send500(connection_id, "Another client already attached");
      return;
    }
    browser_target_.reset(new DevToolsBrowserTarget(
        thread_->message_loop_proxy().get(), server_.get(), connection_id));
    browser_target_->RegisterDomainHandler(
        DevToolsTracingHandler::kDomain, new DevToolsTracingHandler());
    AcceptWebSocket(connection_id, request);
    return;
  }

  size_t pos = request.path.find(kPageUrlPrefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_id = request.path.substr(strlen(kPageUrlPrefix));
  DevToolsAgentHost* agent = binding_->ForIdentifier(page_id);
  if (!agent) {
    Send500(connection_id, "No such target id: " + page_id);
    return;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->GetDevToolsClientHostFor(agent)) {
    Send500(connection_id,
            "Target with given id is being inspected: " + page_id);
    return;
  }

  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(thread_->message_loop(),
                                 server_,
                                 connection_id);
  connection_to_client_host_ui_[connection_id] = client_host;

  manager->RegisterDevToolsClientHostFor(agent, client_host);

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpHandlerImpl::OnWebSocketMessageUI(
    int connection_id,
    const std::string& data) {
  if (browser_target_ && connection_id == browser_target_->connection_id()) {
    std::string json_response = browser_target_->HandleMessage(data);

    thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_.get(),
                   connection_id,
                   json_response));
    return;
  }

  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it == connection_to_client_host_ui_.end())
    return;

  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->DispatchOnInspectorBackend(it->second, data);
}

void DevToolsHttpHandlerImpl::OnCloseUI(int connection_id) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it != connection_to_client_host_ui_.end()) {
    DevToolsClientHostImpl* client_host =
        static_cast<DevToolsClientHostImpl*>(it->second);
    DevToolsManager::GetInstance()->ClientHostClosing(client_host);
    delete client_host;
    connection_to_client_host_ui_.erase(connection_id);
  }
  if (browser_target_ && browser_target_->connection_id() == connection_id) {
    browser_target_.reset();
    return;
  }
}

DevToolsHttpHandlerImpl::DevToolsHttpHandlerImpl(
    const net::StreamListenSocketFactory* socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate)
    : overridden_frontend_url_(frontend_url),
      socket_factory_(socket_factory),
      delegate_(delegate) {
  if (overridden_frontend_url_.empty())
      overridden_frontend_url_ = "/devtools/devtools.html";

  default_binding_.reset(new DevToolsDefaultBindingHandler);
  binding_ = default_binding_.get();

  // Balanced in ResetHandlerThreadAndRelease().
  AddRef();
}

// Runs on the handler thread
void DevToolsHttpHandlerImpl::Init() {
  server_ = new net::HttpServer(*socket_factory_.get(), this);
}

// Runs on the handler thread
void DevToolsHttpHandlerImpl::Teardown() {
  server_ = NULL;
}

// Runs on FILE thread to make sure that it is serialized against
// {Start|Stop}HandlerThread and to allow calling pthread_join.
void DevToolsHttpHandlerImpl::StopHandlerThread() {
  if (!thread_->message_loop())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Teardown, this));
  // Thread::Stop joins the thread.
  thread_->Stop();
}

void DevToolsHttpHandlerImpl::SendJson(int connection_id,
                                       net::HttpStatusCode status_code,
                                       base::Value* value,
                                       const std::string& message) {
  if (!thread_.get())
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

  std::string response;
  std::string mime_type = "application/json; charset=UTF-8";

  response = base::StringPrintf("%s%s", json_value.c_str(), message.c_str());

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send,
                 server_.get(),
                 connection_id,
                 status_code,
                 response,
                 mime_type));
}

void DevToolsHttpHandlerImpl::Send200(int connection_id,
                                      const std::string& data,
                                      const std::string& mime_type) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send200,
                 server_.get(),
                 connection_id,
                 data,
                 mime_type));
}

void DevToolsHttpHandlerImpl::Send404(int connection_id) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send404, server_.get(), connection_id));
}

void DevToolsHttpHandlerImpl::Send500(int connection_id,
                                      const std::string& message) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send500, server_.get(), connection_id,
                 message));
}

void DevToolsHttpHandlerImpl::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::AcceptWebSocket, server_.get(),
                 connection_id, request));
}

base::DictionaryValue* DevToolsHttpHandlerImpl::SerializePageInfo(
    RenderViewHost* rvh,
    const std::string& host) {
  base::DictionaryValue* dictionary = new base::DictionaryValue;

  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(rvh));

  std::string id = binding_->GetIdentifier(agent);
  dictionary->SetString(kTargetIdField, id);

  switch (delegate_->GetTargetType(rvh)) {
    case DevToolsHttpHandlerDelegate::kTargetTypeTab:
      dictionary->SetString(kTargetTypeField, kTargetTypePage);
      break;
    default:
      dictionary->SetString(kTargetTypeField, kTargetTypeOther);
  }

  WebContents* web_contents = rvh->GetDelegate()->GetAsWebContents();
  if (web_contents) {
    dictionary->SetString(kTargetTitleField, UTF16ToUTF8(
        net::EscapeForHTML(web_contents->GetTitle())));
    dictionary->SetString(kTargetUrlField, web_contents->GetURL().spec());
    dictionary->SetString(kTargetThumbnailUrlField,
        std::string(kThumbUrlPrefix) + id);

    NavigationController& controller = web_contents->GetController();
    NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid()) {
      dictionary->SetString(kTargetFaviconUrlField,
          entry->GetFavicon().url.spec());
    }
  }
  dictionary->SetString(kTargetDescriptionField,
      delegate_->GetViewDescription(rvh));

  if (!DevToolsManager::GetInstance()->GetDevToolsClientHostFor(agent))
    SerializeDebuggerURLs(dictionary, id, host);
  return dictionary;
}

base::DictionaryValue* DevToolsHttpHandlerImpl::SerializeWorkerInfo(
    const WorkerService::WorkerInfo& worker,
    const std::string& host) {
  base::DictionaryValue* dictionary = new base::DictionaryValue;

  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetForWorker(
      worker.process_id, worker.route_id));

  std::string id = binding_->GetIdentifier(agent);

  dictionary->SetString(kTargetIdField, id);
  dictionary->SetString(kTargetTypeField, kTargetTypeOther);
  dictionary->SetString(kTargetTitleField,
      UTF16ToUTF8(net::EscapeForHTML(worker.name)));
  dictionary->SetString(kTargetUrlField, worker.url.spec());
  dictionary->SetString(kTargetDescriptionField,
      base::StringPrintf("Worker pid:%d", base::GetProcId(worker.handle)));

  if (!DevToolsManager::GetInstance()->GetDevToolsClientHostFor(agent))
    SerializeDebuggerURLs(dictionary, id, host);
  return dictionary;
}

void DevToolsHttpHandlerImpl::SerializeDebuggerURLs(
    base::DictionaryValue* dictionary,
    const std::string& id,
    const std::string& host) {
  dictionary->SetString(kTargetWebSocketDebuggerUrlField,
                        base::StringPrintf("ws://%s%s%s",
                                           host.c_str(),
                                           kPageUrlPrefix,
                                           id.c_str()));
  std::string devtools_frontend_url = GetFrontendURLInternal(
      id.c_str(),
      host);
  dictionary->SetString(kTargetDevtoolsFrontendUrlField, devtools_frontend_url);
}

}  // namespace content
