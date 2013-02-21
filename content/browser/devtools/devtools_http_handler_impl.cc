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
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
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

class DevToolsDefaultBindingHandler
    : public DevToolsHttpHandler::DevToolsAgentHostBinding {
 public:
  DevToolsDefaultBindingHandler() {
  }

  void GarbageCollect() {
    AgentsMap::iterator it = agents_map_.begin();
    while (it != agents_map_.end()) {
      if (!it->second->GetRenderViewHost())
        agents_map_.erase(it++);
      else
        ++it;
    }
  }

  virtual std::string GetIdentifier(DevToolsAgentHost* agent_host) OVERRIDE {
    GarbageCollect();
    DevToolsAgentHostImpl* agent_host_impl =
        static_cast<DevToolsAgentHostImpl*>(agent_host);
    std::string id = base::StringPrintf("%d", agent_host_impl->id());
    agents_map_[id] = agent_host;
    return id;
  }

  virtual DevToolsAgentHost* ForIdentifier(
      const std::string& identifier) OVERRIDE {
    GarbageCollect();
    AgentsMap::iterator it = agents_map_.find(identifier);
    if (it != agents_map_.end())
      return it->second;
    return NULL;
  }

 private:
  typedef std::map<std::string, scoped_refptr<DevToolsAgentHost> > AgentsMap;
  AgentsMap agents_map_;
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

void DevToolsHttpHandlerImpl::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();
  DevToolsManager* manager = DevToolsManager::GetInstance();
  for (ConnectionToClientHostMap::iterator it =
       connection_to_client_host_ui_.begin();
       it != connection_to_client_host_ui_.end(); ++it) {
    DevToolsAgentHost* agent = manager->GetDevToolsAgentHostFor(it->second);
    if (!agent)
      continue;
    RenderViewHost* rvh = agent->GetRenderViewHost();
    if (rvh && rvh->GetProcess() == process)
      it->second->InspectedContentsClosing();
  }
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

  if (info.path.find("/thumb/") == 0) {
    // Thumbnail request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnThumbnailRequestUI,
                   this,
                   connection_id,
                   info));
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

struct DevToolsHttpHandlerImpl::PageInfo {
  PageInfo()
      : attached(false) {
  }

  std::string id;
  std::string url;
  std::string type;
  bool attached;
  std::string title;
  std::string thumbnail_url;
  std::string favicon_url;
  base::TimeTicks last_selected_time;
};

// static
bool DevToolsHttpHandlerImpl::SortPageListByTime(const PageInfo& info1,
                                                 const PageInfo& info2) {
  return info1.last_selected_time > info2.last_selected_time;
}

DevToolsHttpHandlerImpl::PageList DevToolsHttpHandlerImpl::GeneratePageList() {
  PageList page_list;
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* render_process_host = it.GetCurrentValue();
    DCHECK(render_process_host);

    // Ignore processes that don't have a connection, such as crashed contents.
    if (!render_process_host->HasConnection())
      continue;

    RenderProcessHost::RenderWidgetHostsIterator rwit(
        render_process_host->GetRenderWidgetHostsIterator());
    for (; !rwit.IsAtEnd(); rwit.Advance()) {
      const RenderWidgetHost* widget = rwit.GetCurrentValue();
      DCHECK(widget);
      if (!widget || !widget->IsRenderView())
        continue;

      RenderViewHost* host =
          RenderViewHost::From(const_cast<RenderWidgetHost*>(widget));
      page_list.push_back(CreatePageInfo(host, delegate_->GetTargetType(host)));
    }
  }
  std::sort(page_list.begin(), page_list.end(), SortPageListByTime);
  return page_list;
}

std::string DevToolsHttpHandlerImpl::GetFrontendURLInternal(
    const std::string rvh_id,
    const std::string& host) {
  return base::StringPrintf(
      "%s%sws=%s/devtools/page/%s",
      overridden_frontend_url_.c_str(),
      overridden_frontend_url_.find("?") == std::string::npos ? "?" : "&",
      host.c_str(),
      rvh_id.c_str());
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
  // Trim /json and ?jsonp=...
  std::string path = info.path.substr(5);
  std::string jsonp;
  size_t jsonp_pos = path.find("?jsonp=");
  if (jsonp_pos != std::string::npos) {
    jsonp = path.substr(jsonp_pos + 7);
    path = path.substr(0, jsonp_pos);
  }

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
             "Malformed query: " + info.path,
             jsonp);
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
    SendJson(connection_id, net::HTTP_OK, &version, "", jsonp);
    return;
  }

  if (command == "list") {
    PageList page_list = GeneratePageList();
    base::ListValue json_pages_list;
    std::string host = info.headers["Host"];
    for (PageList::iterator i = page_list.begin(); i != page_list.end(); ++i)
      json_pages_list.Append(SerializePageInfo(*i, host));
    SendJson(connection_id, net::HTTP_OK, &json_pages_list, "", jsonp);
    return;
  }

  if (command == "new") {
    RenderViewHost* rvh = delegate_->CreateNewTarget();
    if (!rvh) {
      SendJson(connection_id,
               net::HTTP_INTERNAL_SERVER_ERROR,
               NULL,
               "Could not create new page",
               jsonp);
      return;
    }
    PageInfo page_info =
        CreatePageInfo(rvh, DevToolsHttpHandlerDelegate::kTargetTypeTab);
    std::string host = info.headers["Host"];
    scoped_ptr<base::DictionaryValue> dictionary(
        SerializePageInfo(page_info, host));
    SendJson(connection_id, net::HTTP_OK, dictionary.get(), "", jsonp);
    return;
  }

  if (command == "activate" || command == "close") {
    DevToolsAgentHost* agent_host = binding_->ForIdentifier(target_id);
    RenderViewHost* rvh = agent_host ? agent_host->GetRenderViewHost() : NULL;
    if (!rvh) {
      SendJson(connection_id,
               net::HTTP_NOT_FOUND,
               NULL,
               "No such target id: " + target_id,
               jsonp);
      return;
    }

    if (command == "activate") {
      rvh->GetDelegate()->Activate();
      SendJson(connection_id, net::HTTP_OK, NULL, "Target activated", jsonp);
      return;
    }

    if (command == "close") {
      rvh->ClosePage();
      SendJson(connection_id, net::HTTP_OK, NULL, "Target is closing", jsonp);
      return;
    }
  }
  SendJson(connection_id,
           net::HTTP_NOT_FOUND,
           NULL,
           "Unknown command: " + command,
           jsonp);
  return;
}

void DevToolsHttpHandlerImpl::OnThumbnailRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  std::string prefix = "/thumb/";
  size_t pos = info.path.find(prefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_url = info.path.substr(prefix.length());
  std::string data = delegate_->GetPageThumbnailData(GURL(page_url));
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

  std::string page_prefix = "/devtools/page/";
  size_t pos = request.path.find(page_prefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_id = request.path.substr(page_prefix.length());
  DevToolsAgentHost* agent_host = binding_->ForIdentifier(page_id);
  RenderViewHost* rvh = agent_host ? agent_host->GetRenderViewHost() : NULL;
  if (!rvh) {
    Send500(connection_id, "No such target id: " + page_id);
    return;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetFor(rvh));
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

  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());

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
                                       const std::string& message,
                                       const std::string& jsonp) {
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

  // Wrap jsonp if necessary.
  if (!jsonp.empty()) {
    mime_type = "text/javascript; charset=UTF-8";
    response = StringPrintf("%s(%s, %d, %s);",
                            jsonp.c_str(),
                            json_value.empty() ? "undefined"
                                               : json_value.c_str(),
                            status_code,
                            json_message.c_str());
    // JSONP always returns 200.
    status_code = net::HTTP_OK;
  } else {
    response = StringPrintf("%s%s", json_value.c_str(), message.c_str());
  }

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

DevToolsHttpHandlerImpl::PageInfo
DevToolsHttpHandlerImpl::CreatePageInfo(RenderViewHost* rvh,
    DevToolsHttpHandlerDelegate::TargetType type) {
  RenderViewHostDelegate* host_delegate = rvh->GetDelegate();
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetFor(rvh));
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(agent);
  PageInfo page_info;
  page_info.id = binding_->GetIdentifier(agent);
  page_info.attached = client_host != NULL;
  page_info.url = host_delegate->GetURL().spec();

  switch (type) {
    case DevToolsHttpHandlerDelegate::kTargetTypeTab:
      page_info.type = "page";
      break;
    default:
      page_info.type = "other";
  }

  WebContents* web_contents = host_delegate->GetAsWebContents();
  if (web_contents) {
    page_info.title = UTF16ToUTF8(
        net::EscapeForHTML(web_contents->GetTitle()));
    page_info.last_selected_time = web_contents->GetLastSelectedTime();

    NavigationController& controller = web_contents->GetController();
    NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid()) {
      page_info.thumbnail_url = "/thumb/" + entry->GetURL().spec();
      page_info.favicon_url = entry->GetFavicon().url.spec();
    }
  }
  return page_info;
}

base::DictionaryValue* DevToolsHttpHandlerImpl::SerializePageInfo(
    const PageInfo& page_info,
    const std::string& host) {
  base::DictionaryValue* dictionary = new base::DictionaryValue;
  dictionary->SetString("title", page_info.title);
  dictionary->SetString("url", page_info.url);
  dictionary->SetString("type", page_info.type);
  dictionary->SetString("id", page_info.id);
  dictionary->SetString("thumbnailUrl", page_info.thumbnail_url);
  dictionary->SetString("faviconUrl", page_info.favicon_url);
  if (!page_info.attached) {
    dictionary->SetString("webSocketDebuggerUrl",
                          base::StringPrintf("ws://%s/devtools/page/%s",
                                             host.c_str(),
                                             page_info.id.c_str()));
    std::string devtools_frontend_url = GetFrontendURLInternal(
        page_info.id.c_str(),
        host);
    dictionary->SetString("devtoolsFrontendUrl", devtools_frontend_url);
  }
  return dictionary;
}

}  // namespace content
