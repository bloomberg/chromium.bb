// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/devtools_android_bridge.h"

#include <map>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"
#include "chrome/browser/devtools/device/adb/adb_device_info_query.h"
#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/self_device_provider.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/user_metrics.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace {

const char kModelOffline[] = "Offline";

const char kPageListRequest[] = "/json";
const char kVersionRequest[] = "/json/version";
const char kClosePageRequest[] = "/json/close/%s";
const char kNewPageRequest[] = "/json/new";
const char kNewPageRequestWithURL[] = "/json/new?%s";
const char kActivatePageRequest[] = "/json/activate/%s";
const char kBrowserTargetSocket[] = "/devtools/browser";
const int kAdbPollingIntervalMs = 1000;

const char kUrlParam[] = "url";
const char kPageReloadCommand[] = "Page.reload";
const char kPageNavigateCommand[] = "Page.navigate";

const int kMinVersionNewWithURL = 32;
const int kNewPageNavigateDelayMs = 500;

// DiscoveryRequest -----------------------------------------------------

class DiscoveryRequest : public base::RefCountedThreadSafe<
    DiscoveryRequest,
    BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(DevToolsAndroidBridge::RemoteDevices*)> Callback;

  DiscoveryRequest(
      scoped_refptr<DevToolsAndroidBridge> android_bridge,
      AndroidDeviceManager* device_manager,
      base::MessageLoop* device_message_loop,
      const AndroidDeviceManager::DeviceProviders& device_providers,
      const Callback& callback);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<DiscoveryRequest>;

  virtual ~DiscoveryRequest();

  void ReceivedSerials(const std::vector<std::string>& serials);
  void ProcessSerials();
  void ReceivedDeviceInfo(const AndroidDeviceManager::DeviceInfo& device_info);
  void ProcessSockets();
  void ReceivedVersion(int result, const std::string& response);
  void ReceivedPages(int result, const std::string& response);

  std::string current_serial() const { return serials_.back(); }

  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> current_browser() const {
    return browsers_.back();
  }

  void NextBrowser();
  void NextDevice();

  void Respond();

  scoped_refptr<DevToolsAndroidBridge> android_bridge_;
  AndroidDeviceManager* device_manager_;
  base::MessageLoop* device_message_loop_;
  Callback callback_;
  std::vector<std::string> serials_;
  DevToolsAndroidBridge::RemoteBrowsers browsers_;
  scoped_ptr<DevToolsAndroidBridge::RemoteDevices> remote_devices_;
};

DiscoveryRequest::DiscoveryRequest(
    scoped_refptr<DevToolsAndroidBridge> android_bridge,
    AndroidDeviceManager* device_manager,
    base::MessageLoop* device_message_loop,
    const AndroidDeviceManager::DeviceProviders& device_providers,
    const Callback& callback)
    : android_bridge_(android_bridge),
      device_manager_(device_manager),
      device_message_loop_(device_message_loop),
      callback_(callback) {
  remote_devices_.reset(new DevToolsAndroidBridge::RemoteDevices());

  device_message_loop_->PostTask(
      FROM_HERE, base::Bind(
          &AndroidDeviceManager::QueryDevices,
          device_manager_,
          device_providers,
          base::Bind(&DiscoveryRequest::ReceivedSerials, this)));
}

DiscoveryRequest::~DiscoveryRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DiscoveryRequest::ReceivedSerials(
    const std::vector<std::string>& serials) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  serials_ = serials;
  ProcessSerials();
}

void DiscoveryRequest::ProcessSerials() {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (serials_.size() == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DiscoveryRequest::Respond, this));
    return;
  }

  if (device_manager_->IsConnected(current_serial())) {
    device_manager_->QueryDeviceInfo(current_serial(),
        base::Bind(&DiscoveryRequest::ReceivedDeviceInfo, this));
  } else {
    AndroidDeviceManager::DeviceInfo offline_info;
    offline_info.model = kModelOffline;
    remote_devices_->push_back(new DevToolsAndroidBridge::RemoteDevice(
        android_bridge_, current_serial(), offline_info, false));
    NextDevice();
  }
}

void DiscoveryRequest::ReceivedDeviceInfo(
    const AndroidDeviceManager::DeviceInfo& device_info) {
  remote_devices_->push_back(new DevToolsAndroidBridge::RemoteDevice(
          android_bridge_, current_serial(), device_info, true));
  browsers_ = remote_devices_->back()->browsers();
  ProcessSockets();
}

void DiscoveryRequest::ProcessSockets() {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (browsers_.size() == 0) {
    NextDevice();
    return;
  }

  device_manager_->HttpQuery(
      current_serial(),
      current_browser()->socket(),
      kVersionRequest,
      base::Bind(&DiscoveryRequest::ReceivedVersion, this));
}

void DiscoveryRequest::ReceivedVersion(int result,
                                       const std::string& response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result < 0) {
    NextBrowser();
    return;
  }

  // Parse version, append to package name if available,
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    std::string browser;
    if (dict->GetString("Browser", &browser)) {
      std::vector<std::string> parts;
      Tokenize(browser, "/", &parts);
      if (parts.size() == 2)
        current_browser()->set_version(parts[1]);
      else
        current_browser()->set_version(browser);
    }
    std::string package;
    if (dict->GetString("Android-Package", &package)) {
      current_browser()->set_display_name(
          AdbDeviceInfoQuery::GetDisplayName(current_browser()->socket(),
                                             package));
    }
  }

  device_manager_->HttpQuery(
      current_serial(),
      current_browser()->socket(),
      kPageListRequest,
      base::Bind(&DiscoveryRequest::ReceivedPages, this));
}

void DiscoveryRequest::ReceivedPages(int result,
                                     const std::string& response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result >= 0) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(response));
    base::ListValue* list_value;
    if (value && value->GetAsList(&list_value))
      current_browser()->SetPageDescriptors(*list_value);
  }
  NextBrowser();
}

void DiscoveryRequest::NextBrowser() {
  browsers_.pop_back();
  ProcessSockets();
}

void DiscoveryRequest::NextDevice() {
  serials_.pop_back();
  ProcessSerials();
}

void DiscoveryRequest::Respond() {
  callback_.Run(remote_devices_.release());
}

// ProtocolCommand ------------------------------------------------------------

class ProtocolCommand
    : public DevToolsAndroidBridge::AndroidWebSocket::Delegate {
 public:
  ProtocolCommand(
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
      const std::string& debug_url,
      const std::string& command,
      const base::Closure callback);

 private:
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;

  const std::string command_;
  const base::Closure callback_;
  scoped_refptr<DevToolsAndroidBridge::AndroidWebSocket> web_socket_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolCommand);
};

ProtocolCommand::ProtocolCommand(
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const std::string& debug_url,
    const std::string& command,
    const base::Closure callback)
    : command_(command),
      callback_(callback){
  web_socket_ = browser->CreateWebSocket(debug_url, this);
  web_socket_->Connect();
}

void ProtocolCommand::OnSocketOpened() {
  web_socket_->SendFrame(command_);
}

void ProtocolCommand::OnFrameRead(const std::string& message) {
  web_socket_->Disconnect();
}

void ProtocolCommand::OnSocketClosed(bool closed_by_device) {
  if (!callback_.is_null()) {
    callback_.Run();
  }
  delete this;
}

}  // namespace

class AgentHostDelegate;

typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;

base::LazyInstance<AgentHostDelegates>::Leaky g_host_delegates =
    LAZY_INSTANCE_INITIALIZER;

DevToolsAndroidBridge::Wrapper::Wrapper(content::BrowserContext* context) {
  bridge_ = new DevToolsAndroidBridge(Profile::FromBrowserContext(context));
}

DevToolsAndroidBridge::Wrapper::~Wrapper() {
}

DevToolsAndroidBridge* DevToolsAndroidBridge::Wrapper::Get() {
  return bridge_.get();
}

// static
DevToolsAndroidBridge::Factory* DevToolsAndroidBridge::Factory::GetInstance() {
  return Singleton<DevToolsAndroidBridge::Factory>::get();
}

// static
DevToolsAndroidBridge* DevToolsAndroidBridge::Factory::GetForProfile(
    Profile* profile) {
  DevToolsAndroidBridge::Wrapper* wrapper =
      static_cast<DevToolsAndroidBridge::Wrapper*>(GetInstance()->
          GetServiceForBrowserContext(profile, true));
  return wrapper ? wrapper->Get() : NULL;
}

DevToolsAndroidBridge::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "DevToolsAndroidBridge",
          BrowserContextDependencyManager::GetInstance()) {}

DevToolsAndroidBridge::Factory::~Factory() {}

KeyedService* DevToolsAndroidBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevToolsAndroidBridge::Wrapper(context);
}


// AgentHostDelegate ----------------------------------------------------------

class AgentHostDelegate
    : public content::DevToolsExternalAgentProxyDelegate,
      public DevToolsAndroidBridge::AndroidWebSocket::Delegate {
 public:
  static scoped_refptr<content::DevToolsAgentHost> GetOrCreateAgentHost(
      const std::string& id,
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
      const std::string& debug_url);

 private:
  AgentHostDelegate(
      const std::string& id,
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
      const std::string& debug_url);
  virtual ~AgentHostDelegate();
  virtual void Attach(content::DevToolsExternalAgentProxy* proxy) OVERRIDE;
  virtual void Detach() OVERRIDE;
  virtual void SendMessageToBackend(
      const std::string& message) OVERRIDE;
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;

  const std::string id_;
  bool socket_opened_;
  bool detached_;
  bool is_web_view_;
  std::vector<std::string> pending_messages_;
  scoped_refptr<DevToolsAndroidBridge::AndroidWebSocket> web_socket_;
  content::DevToolsAgentHost* agent_host_;
  content::DevToolsExternalAgentProxy* proxy_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

// static
scoped_refptr<content::DevToolsAgentHost>
AgentHostDelegate::GetOrCreateAgentHost(
    const std::string& id,
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const std::string& debug_url) {
   DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
   AgentHostDelegates::iterator it = g_host_delegates.Get().find(id);
   if (it != g_host_delegates.Get().end())
     return it->second->agent_host_;

  AgentHostDelegate* delegate = new AgentHostDelegate(id, browser, debug_url);
  scoped_refptr<content::DevToolsAgentHost> result =
      content::DevToolsAgentHost::Create(delegate);
  delegate->agent_host_ = result.get();
  return result;
}

AgentHostDelegate::AgentHostDelegate(
    const std::string& id,
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const std::string& debug_url)
    : id_(id),
      socket_opened_(false),
      detached_(false),
      is_web_view_(browser->IsWebView()),
      web_socket_(browser->CreateWebSocket(debug_url, this)),
      agent_host_(NULL),
      proxy_(NULL) {
  g_host_delegates.Get()[id] = this;
}

AgentHostDelegate::~AgentHostDelegate() {
  g_host_delegates.Get().erase(id_);
  web_socket_->ClearDelegate();
}

void AgentHostDelegate::Attach(content::DevToolsExternalAgentProxy* proxy) {
  proxy_ = proxy;
  content::RecordAction(base::UserMetricsAction(is_web_view_ ?
      "DevTools_InspectAndroidWebView" : "DevTools_InspectAndroidPage"));
  web_socket_->Connect();
}

void AgentHostDelegate::Detach() {
  detached_ = true;
  if (socket_opened_)
    web_socket_->Disconnect();
}

void AgentHostDelegate::SendMessageToBackend(const std::string& message) {
  if (socket_opened_)
    web_socket_->SendFrame(message);
  else
    pending_messages_.push_back(message);
}

void AgentHostDelegate::OnSocketOpened() {
  if (detached_) {
    web_socket_->Disconnect();
    return;
  }

  socket_opened_ = true;
  for (std::vector<std::string>::iterator it = pending_messages_.begin();
       it != pending_messages_.end(); ++it) {
    SendMessageToBackend(*it);
  }
  pending_messages_.clear();
}

void AgentHostDelegate::OnFrameRead(const std::string& message) {
  if (proxy_)
      proxy_->DispatchOnClientHost(message);
}

void AgentHostDelegate::OnSocketClosed(bool closed_by_device) {
  if (proxy_ && closed_by_device)
    proxy_->ConnectionClosed();
}

//// RemotePageTarget ----------------------------------------------

class RemotePageTarget : public DevToolsTargetImpl,
                         public DevToolsAndroidBridge::RemotePage {
 public:
  RemotePageTarget(scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
                   const base::DictionaryValue& value);
  virtual ~RemotePageTarget();

  // DevToolsAndroidBridge::RemotePage implementation.
  virtual DevToolsTargetImpl* GetTarget() OVERRIDE;
  virtual std::string GetFrontendURL() OVERRIDE;

  // DevToolsTargetImpl overrides.
  virtual bool IsAttached() const OVERRIDE;
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;
  virtual void Inspect(Profile* profile) const OVERRIDE;
  virtual void Reload() const OVERRIDE;

  void Navigate(const std::string& url, base::Closure callback) const;

 private:
  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser_;
  std::string debug_url_;
  std::string frontend_url_;
  std::string remote_id_;
  DISALLOW_COPY_AND_ASSIGN(RemotePageTarget);
};

static std::string GetStringProperty(const base::DictionaryValue& value,
                                     const std::string& name) {
  std::string result;
  value.GetString(name, &result);
  return result;
}

static std::string BuildUniqueTargetId(
    DevToolsAndroidBridge::RemoteBrowser* browser,
    const base::DictionaryValue& value) {
  return base::StringPrintf("%s:%s:%s", browser->serial().c_str(),
      browser->socket().c_str(), GetStringProperty(value, "id").c_str());
}

static std::string GetDebugURL(const base::DictionaryValue& value) {
  std::string debug_url = GetStringProperty(value, "webSocketDebuggerUrl");

  if (debug_url.find("ws://") == 0)
    debug_url = debug_url.substr(5);
  else
    debug_url = "";
  return debug_url;
}

RemotePageTarget::RemotePageTarget(
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const base::DictionaryValue& value)
    : DevToolsTargetImpl(AgentHostDelegate::GetOrCreateAgentHost(
                             BuildUniqueTargetId(browser.get(), value),
                             browser, GetDebugURL(value))),
      browser_(browser),
      debug_url_(GetDebugURL(value)),
      remote_id_(GetStringProperty(value, "id")) {
  set_type("adb_page");
  set_url(GURL(GetStringProperty(value, "url")));
  set_title(base::UTF16ToUTF8(net::UnescapeForHTML(base::UTF8ToUTF16(
      GetStringProperty(value, "title")))));
  set_description(GetStringProperty(value, "description"));
  set_favicon_url(GURL(GetStringProperty(value, "faviconUrl")));
  debug_url_ = GetDebugURL(value);
  frontend_url_ = GetStringProperty(value, "devtoolsFrontendUrl");

  size_t ws_param = frontend_url_.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url_ = frontend_url_.substr(0, ws_param);
  if (frontend_url_.find("http:") == 0)
    frontend_url_ = "https:" + frontend_url_.substr(5);
}

RemotePageTarget::~RemotePageTarget() {
}

DevToolsTargetImpl* RemotePageTarget::GetTarget() {
  return this;
}

std::string RemotePageTarget::GetFrontendURL() {
  return frontend_url_;
}

bool RemotePageTarget::IsAttached() const {
  return debug_url_.empty();
}

static void NoOp(int, const std::string&) {}

void RemotePageTarget::Inspect(Profile* profile) const {
  Activate();
  DevToolsWindow::OpenExternalFrontend(profile, frontend_url_,
                                       GetAgentHost());
}

bool RemotePageTarget::Activate() const {
  std::string request = base::StringPrintf(kActivatePageRequest,
                                           remote_id_.c_str());
  browser_->SendJsonRequest(request, base::Bind(&NoOp));
  return true;
}

bool RemotePageTarget::Close() const {
  std::string request = base::StringPrintf(kClosePageRequest,
                                           remote_id_.c_str());
  browser_->SendJsonRequest(request, base::Bind(&NoOp));
  return true;
}

void RemotePageTarget::Reload() const {
  browser_->SendProtocolCommand(debug_url_, kPageReloadCommand, NULL,
                                base::Closure());
}

void RemotePageTarget::Navigate(const std::string& url,
                                base::Closure callback) const {
  base::DictionaryValue params;
  params.SetString(kUrlParam, url);
  browser_->SendProtocolCommand(debug_url_, kPageNavigateCommand, &params,
                                callback);
}

// DevToolsAndroidBridge::RemoteBrowser ---------------------------------------

DevToolsAndroidBridge::RemoteBrowser::RemoteBrowser(
    scoped_refptr<DevToolsAndroidBridge> android_bridge,
    const std::string& serial,
    const AndroidDeviceManager::BrowserInfo& browser_info)
    : android_bridge_(android_bridge),
      serial_(serial),
      socket_(browser_info.socket_name),
      display_name_(browser_info.display_name),
      type_(browser_info.type),
      page_descriptors_(new base::ListValue()) {
}

bool DevToolsAndroidBridge::RemoteBrowser::IsChrome() const {
  return type_ == AndroidDeviceManager::BrowserInfo::kTypeChrome;
}

bool DevToolsAndroidBridge::RemoteBrowser::IsWebView() const {
  return type_ == AndroidDeviceManager::BrowserInfo::kTypeWebView;
}

DevToolsAndroidBridge::RemoteBrowser::ParsedVersion
DevToolsAndroidBridge::RemoteBrowser::GetParsedVersion() const {
  ParsedVersion result;
  std::vector<std::string> parts;
  Tokenize(version_, ".", &parts);
  for (size_t i = 0; i != parts.size(); ++i) {
    int value = 0;
    base::StringToInt(parts[i], &value);
    result.push_back(value);
  }
  return result;
}

std::vector<DevToolsAndroidBridge::RemotePage*>
DevToolsAndroidBridge::RemoteBrowser::CreatePages() {
  std::vector<DevToolsAndroidBridge::RemotePage*> result;
  for (size_t i = 0; i < page_descriptors_->GetSize(); ++i) {
    base::Value* item;
    page_descriptors_->Get(i, &item);
    if (!item)
      continue;
    base::DictionaryValue* dict;
    if (!item->GetAsDictionary(&dict))
      continue;
    result.push_back(new RemotePageTarget(this, *dict));
  }
  return result;
}

void DevToolsAndroidBridge::RemoteBrowser::SetPageDescriptors(
    const base::ListValue& list) {
  page_descriptors_.reset(list.DeepCopy());
}

static void RespondOnUIThread(
    const DevToolsAndroidBridge::RemoteBrowser::JsonRequestCallback& callback,
    int result,
    const std::string& response) {
  if (callback.is_null())
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result, response));
}

void DevToolsAndroidBridge::RemoteBrowser::SendJsonRequest(
    const std::string& request, const JsonRequestCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  android_bridge_->device_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidDeviceManager::HttpQuery,
                 android_bridge_->device_manager(), serial_, socket_, request,
                 base::Bind(&RespondOnUIThread, callback)));
}

void DevToolsAndroidBridge::RemoteBrowser::SendProtocolCommand(
    const std::string& debug_url,
    const std::string& method,
    base::DictionaryValue* params,
    const base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (debug_url.empty())
    return;
  DevToolsProtocol::Command command(1, method, params);
  new ProtocolCommand(this, debug_url, command.Serialize(), callback);
}

void DevToolsAndroidBridge::RemoteBrowser::Open(
    const std::string& url,
    const DevToolsAndroidBridge::RemotePageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InnerOpen(url, base::Bind(&RemoteBrowser::RespondToOpenOnUIThread,
                            this, callback));
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::RemoteBrowser::GetAgentHost() {
  return AgentHostDelegate::GetOrCreateAgentHost(
      "adb:" + serial_ + ":" + socket_, this, kBrowserTargetSocket);
}

void DevToolsAndroidBridge::RemoteBrowser::RespondToOpenOnUIThread(
    const DevToolsAndroidBridge::RemotePageCallback& callback,
    int result,
    const std::string& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (result < 0) {
    callback.Run(NULL);
    return;
  }
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    RemotePageTarget* new_page = new RemotePageTarget(this, *dict);
    callback.Run(new_page);
  }
}

void DevToolsAndroidBridge::RemoteBrowser::InnerOpen(
    const std::string& input_url,
    const JsonRequestCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GURL gurl(input_url);
  if (!gurl.is_valid()) {
    gurl = GURL("http://" + input_url);
    if (!gurl.is_valid())
     return;
  }
  std::string url = gurl.spec();

  ParsedVersion parsed_version = GetParsedVersion();
  if (IsChrome() &&
      !parsed_version.empty() &&
      parsed_version[0] >= kMinVersionNewWithURL) {
    std::string query = net::EscapeQueryParamValue(url, false /* use_plus */);
    std::string request =
        base::StringPrintf(kNewPageRequestWithURL, query.c_str());
    SendJsonRequest(request, callback);
  } else {
    SendJsonRequest(kNewPageRequest,
        base::Bind(&RemoteBrowser::PageCreatedOnUIThread, this,
                   callback, url));
  }
}

void DevToolsAndroidBridge::RemoteBrowser::PageCreatedOnUIThread(
    const JsonRequestCallback& callback,
    const std::string& url, int result, const std::string& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (result < 0)
    return;
  // Navigating too soon after the page creation breaks navigation history
  // (crbug.com/311014). This can be avoided by adding a moderate delay.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemoteBrowser::NavigatePageOnUIThread,
                 this, callback, result, response, url),
      base::TimeDelta::FromMilliseconds(kNewPageNavigateDelayMs));
}

void DevToolsAndroidBridge::RemoteBrowser::NavigatePageOnUIThread(
    const JsonRequestCallback& callback,
    int result, const std::string& response, const std::string& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;

  if (value && value->GetAsDictionary(&dict)) {
    RemotePageTarget new_page(this, *dict);
    new_page.Navigate(url,
        base::Bind(&RespondOnUIThread, callback, result, response));
  }
}

DevToolsAndroidBridge::RemoteBrowser::~RemoteBrowser() {
}


// DevToolsAndroidBridge::RemoteDevice ----------------------------------------

DevToolsAndroidBridge::RemoteDevice::RemoteDevice(
    scoped_refptr<DevToolsAndroidBridge> android_bridge,
    const std::string& serial,
    const AndroidDeviceManager::DeviceInfo& device_info,
    bool connected)
    : android_bridge_(android_bridge),
      serial_(serial),
      model_(device_info.model),
      connected_(connected),
      screen_size_(device_info.screen_size) {
  for (std::vector<AndroidDeviceManager::BrowserInfo>::const_iterator it =
      device_info.browser_info.begin();
      it != device_info.browser_info.end();
      ++it) {
    browsers_.push_back(new DevToolsAndroidBridge::RemoteBrowser(
        android_bridge_, serial_, *it));
  }
}

void DevToolsAndroidBridge::RemoteDevice::OpenSocket(
    const std::string& socket_name,
    const AndroidDeviceManager::SocketCallback& callback) {
  android_bridge_->device_message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDeviceManager::OpenSocket,
                 android_bridge_->device_manager(),
                 serial_,
                 socket_name,
                 callback));
}

DevToolsAndroidBridge::RemoteDevice::~RemoteDevice() {
}

// DevToolsAndroidBridge::HandlerThread ---------------------------------

const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";

DevToolsAndroidBridge::HandlerThread*
DevToolsAndroidBridge::HandlerThread::instance_ = NULL;

// static
scoped_refptr<DevToolsAndroidBridge::HandlerThread>
DevToolsAndroidBridge::HandlerThread::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!instance_)
    new HandlerThread();
  return instance_;
}

DevToolsAndroidBridge::HandlerThread::HandlerThread() {
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = NULL;
  }
}

base::MessageLoop* DevToolsAndroidBridge::HandlerThread::message_loop() {
  return thread_ ? thread_->message_loop() : NULL;
}

// static
void DevToolsAndroidBridge::HandlerThread::StopThread(
    base::Thread* thread) {
  thread->Stop();
}

DevToolsAndroidBridge::HandlerThread::~HandlerThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = NULL;
  if (!thread_)
    return;
  // Shut down thread on FILE thread to join into IO.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&HandlerThread::StopThread, thread_));
}

// DevToolsAndroidBridge ------------------------------------------------------

DevToolsAndroidBridge::DevToolsAndroidBridge(Profile* profile)
    : profile_(profile),
      handler_thread_(HandlerThread::GetInstance()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&DevToolsAndroidBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  CreateDeviceProviders();
  base::PostTaskAndReplyWithResult(
      device_message_loop()->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&AndroidDeviceManager::Create),
      base::Bind(&DevToolsAndroidBridge::CreatedDeviceManager, this));
}

void DevToolsAndroidBridge::AddDeviceListListener(
    DeviceListListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_list_listeners_.push_back(listener);
  if (device_list_listeners_.size() == 1 && device_manager_)
    RequestDeviceList();
}

void DevToolsAndroidBridge::RemoveDeviceListListener(
    DeviceListListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceListListeners::iterator it = std::find(
      device_list_listeners_.begin(), device_list_listeners_.end(), listener);
  DCHECK(it != device_list_listeners_.end());
  device_list_listeners_.erase(it);
  if (device_list_listeners_.empty() && device_manager_) {
    device_message_loop()->PostTask(FROM_HERE,
        base::Bind(&AndroidDeviceManager::Stop, device_manager_));
  }
}

void DevToolsAndroidBridge::AddDeviceCountListener(
    DeviceCountListener* listener) {
  device_count_listeners_.push_back(listener);
  if (device_count_listeners_.size() == 1 && device_manager_)
    RequestDeviceCount();
}

void DevToolsAndroidBridge::RemoveDeviceCountListener(
    DeviceCountListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceCountListeners::iterator it = std::find(
      device_count_listeners_.begin(), device_count_listeners_.end(), listener);
  DCHECK(it != device_count_listeners_.end());
  device_count_listeners_.erase(it);
}

// static
bool DevToolsAndroidBridge::HasDevToolsWindow(const std::string& agent_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_host_delegates.Get().find(agent_id) != g_host_delegates.Get().end();
}

DevToolsAndroidBridge::~DevToolsAndroidBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device_list_listeners_.empty());
  DCHECK(device_count_listeners_.empty());
  if (device_manager_) {
    AndroidDeviceManager* raw_ptr = device_manager_.get();
    device_manager_->AddRef();
    device_manager_ = NULL;
    device_message_loop()->ReleaseSoon(FROM_HERE, raw_ptr);
  }
}

void DevToolsAndroidBridge::RequestDeviceList() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device_manager_);

  if (device_list_listeners_.empty())
    return;

  new DiscoveryRequest(
      this,
      device_manager(),
      device_message_loop(),
      device_providers_,
      base::Bind(&DevToolsAndroidBridge::ReceivedDeviceList, this));
}

void DevToolsAndroidBridge::CreatedDeviceManager(
    scoped_refptr<AndroidDeviceManager> device_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_manager_ = device_manager;
  if (!device_list_listeners_.empty())
    RequestDeviceList();
  if (!device_count_listeners_.empty())
    RequestDeviceCount();
}

void DevToolsAndroidBridge::ReceivedDeviceList(RemoteDevices* devices_ptr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<RemoteDevices> devices(devices_ptr);

  if (device_list_listeners_.empty())
    return;

  DeviceListListeners copy(device_list_listeners_);
  for (DeviceListListeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->DeviceListChanged(*devices.get());

  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsAndroidBridge::RequestDeviceList, this),
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}

void DevToolsAndroidBridge::RequestDeviceCount() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device_manager_);

  if (device_count_listeners_.empty())
    return;

  UsbDeviceProvider::CountDevices(
      base::Bind(&DevToolsAndroidBridge::ReceivedDeviceCount, this));
}

void DevToolsAndroidBridge::ReceivedDeviceCount(int count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (device_count_listeners_.empty())
     return;

  DeviceCountListeners copy(device_count_listeners_);
  for (DeviceCountListeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->DeviceCountChanged(count);

  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsAndroidBridge::RequestDeviceCount, this),
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}

void DevToolsAndroidBridge::CreateDeviceProviders() {
  device_providers_.clear();
#if defined(DEBUG_DEVTOOLS)
  BrowserListTabContentsProvider::EnableTethering();
  // We cannot rely on command line switch here as we might want to connect
  // to another instance of Chrome. Using hard-coded port number instead.
  const int kDefaultDebuggingPort = 9222;
  device_providers_.push_back(new SelfAsDeviceProvider(kDefaultDebuggingPort));
#endif
  device_providers_.push_back(new AdbDeviceProvider());

  PrefService* service = profile_->GetPrefs();
  const PrefService::Preference* pref =
      service->FindPreference(prefs::kDevToolsDiscoverUsbDevicesEnabled);
  const base::Value* pref_value = pref->GetValue();

  bool enabled;
  if (pref_value->GetAsBoolean(&enabled) && enabled) {
    device_providers_.push_back(new UsbDeviceProvider(profile_));
  }
}
