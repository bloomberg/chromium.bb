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
#include "chrome/browser/devtools/device/port_forwarding_controller.h"
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
  typedef AndroidDeviceManager::Device Device;
  typedef AndroidDeviceManager::Devices Devices;
  typedef AndroidDeviceManager::DeviceInfo DeviceInfo;
  typedef DevToolsAndroidBridge::RemoteDevice RemoteDevice;
  typedef DevToolsAndroidBridge::RemoteDevices RemoteDevices;
  typedef DevToolsAndroidBridge::RemoteBrowser RemoteBrowser;
  typedef DevToolsAndroidBridge::RemoteBrowsers RemoteBrowsers;
  typedef base::Callback<void(const RemoteDevices&)> DiscoveryCallback;

  DiscoveryRequest(AndroidDeviceManager* device_manager,
                   const DiscoveryCallback& callback);
 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<DiscoveryRequest>;
  virtual ~DiscoveryRequest();

  void ReceivedDevices(const Devices& devices);
  void ReceivedDeviceInfo(scoped_refptr<Device> device,
                          const DeviceInfo& device_info);
  void ReceivedVersion(scoped_refptr<RemoteBrowser>,
                       int result,
                       const std::string& response);
  void ReceivedPages(scoped_refptr<RemoteBrowser>,
                     int result,
                     const std::string& response);

  DiscoveryCallback callback_;
  RemoteDevices remote_devices_;
};

DiscoveryRequest::DiscoveryRequest(
    AndroidDeviceManager* device_manager,
    const DiscoveryCallback& callback)
    : callback_(callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_manager->QueryDevices(
      base::Bind(&DiscoveryRequest::ReceivedDevices, this));
}

DiscoveryRequest::~DiscoveryRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_.Run(remote_devices_);
}

void DiscoveryRequest::ReceivedDevices(const Devices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (Devices::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    (*it)->QueryDeviceInfo(
        base::Bind(&DiscoveryRequest::ReceivedDeviceInfo, this, *it));
  }
}

void DiscoveryRequest::ReceivedDeviceInfo(scoped_refptr<Device> device,
                                          const DeviceInfo& device_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<RemoteDevice> remote_device =
      new RemoteDevice(device, device_info);
  remote_devices_.push_back(remote_device);
  for (RemoteBrowsers::iterator it = remote_device->browsers().begin();
       it != remote_device->browsers().end(); ++it) {
    (*it)->SendJsonRequest(
        kVersionRequest,
        base::Bind(&DiscoveryRequest::ReceivedVersion, this, *it));
    (*it)->SendJsonRequest(
        kPageListRequest,
        base::Bind(&DiscoveryRequest::ReceivedPages, this, *it));
  }
}

void DiscoveryRequest::ReceivedVersion(scoped_refptr<RemoteBrowser> browser,
                                       int result,
                                       const std::string& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (result < 0)
    return;
  // Parse version, append to package name if available,
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    std::string browser_name;
    if (dict->GetString("Browser", &browser_name)) {
      std::vector<std::string> parts;
      Tokenize(browser_name, "/", &parts);
      if (parts.size() == 2)
        browser->set_version(parts[1]);
      else
        browser->set_version(browser_name);
    }
    std::string package;
    if (dict->GetString("Android-Package", &package)) {
      browser->set_display_name(
          AdbDeviceInfoQuery::GetDisplayName(browser->socket(), package));
    }
  }
}

void DiscoveryRequest::ReceivedPages(scoped_refptr<RemoteBrowser> browser,
                                     int result,
                                     const std::string& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (result < 0)
    return;
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::ListValue* list_value;
  if (value && value->GetAsList(&list_value))
    browser->SetPageDescriptors(*list_value);
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
  virtual void OnSocketClosed() OVERRIDE;
  virtual ~ProtocolCommand();

  const std::string command_;
  const base::Closure callback_;
  scoped_ptr<DevToolsAndroidBridge::AndroidWebSocket> web_socket_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolCommand);
};

ProtocolCommand::ProtocolCommand(
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const std::string& debug_url,
    const std::string& command,
    const base::Closure callback)
    : command_(command),
      callback_(callback),
      web_socket_(browser->CreateWebSocket(debug_url, this)) {
}

void ProtocolCommand::OnSocketOpened() {
  web_socket_->SendFrame(command_);
}

void ProtocolCommand::OnFrameRead(const std::string& message) {
  delete this;
}

void ProtocolCommand::OnSocketClosed() {
  delete this;
}

ProtocolCommand::~ProtocolCommand() {
  if (!callback_.is_null())
    callback_.Run();
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
  virtual void OnSocketClosed() OVERRIDE;

  const std::string id_;
  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser_;
  const std::string debug_url_;
  bool socket_opened_;
  bool is_web_view_;
  std::vector<std::string> pending_messages_;
  scoped_ptr<DevToolsAndroidBridge::AndroidWebSocket> web_socket_;
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
      browser_(browser),
      debug_url_(debug_url),
      socket_opened_(false),
      is_web_view_(browser->IsWebView()),
      agent_host_(NULL),
      proxy_(NULL) {
  g_host_delegates.Get()[id] = this;
}

AgentHostDelegate::~AgentHostDelegate() {
  g_host_delegates.Get().erase(id_);
}

void AgentHostDelegate::Attach(content::DevToolsExternalAgentProxy* proxy) {
  proxy_ = proxy;
  content::RecordAction(base::UserMetricsAction(is_web_view_ ?
      "DevTools_InspectAndroidWebView" : "DevTools_InspectAndroidPage"));
  web_socket_.reset(browser_->CreateWebSocket(debug_url_, this));
}

void AgentHostDelegate::Detach() {
  web_socket_.reset();
}

void AgentHostDelegate::SendMessageToBackend(const std::string& message) {
  if (socket_opened_)
    web_socket_->SendFrame(message);
  else
    pending_messages_.push_back(message);
}

void AgentHostDelegate::OnSocketOpened() {
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

void AgentHostDelegate::OnSocketClosed() {
  if (proxy_)
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
  virtual std::string GetId() const OVERRIDE;
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
  std::string remote_type_;
  std::string local_id_;
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
      remote_id_(GetStringProperty(value, "id")),
      remote_type_(GetStringProperty(value, "type")),
      local_id_(BuildUniqueTargetId(browser.get(), value)) {
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

std::string RemotePageTarget::GetId() const {
  return local_id_;
}

bool RemotePageTarget::IsAttached() const {
  return debug_url_.empty();
}

static void NoOp(int, const std::string&) {}

void RemotePageTarget::Inspect(Profile* profile) const {
  Activate();
  bool isWorker = remote_type_ == kTargetTypeWorker ||
                  remote_type_ == kTargetTypeServiceWorker;
  DevToolsWindow::OpenExternalFrontend(profile, frontend_url_, GetAgentHost(),
                                       isWorker);
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
    scoped_refptr<Device> device,
    const AndroidDeviceManager::BrowserInfo& browser_info)
    : device_(device),
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
    const DevToolsAndroidBridge::JsonRequestCallback& callback,
    int result,
    const std::string& response) {
  if (callback.is_null())
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result, response));
}

void DevToolsAndroidBridge::RemoteBrowser::SendJsonRequest(
    const std::string& request, const JsonRequestCallback& callback) {
  device_->SendJsonRequest(socket_, request, callback);
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
      "adb:" + device_->serial() + ":" + socket_, this, kBrowserTargetSocket);
}

DevToolsAndroidBridge::AndroidWebSocket*
DevToolsAndroidBridge::RemoteBrowser::CreateWebSocket(
    const std::string& url,
    DevToolsAndroidBridge::AndroidWebSocket::Delegate* delegate) {
  return device_->CreateWebSocket(socket_, url, delegate);
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
    scoped_refptr<AndroidDeviceManager::Device> device,
    const AndroidDeviceManager::DeviceInfo& device_info)
    : device_(device),
      model_(device_info.model),
      connected_(device_info.connected),
      screen_size_(device_info.screen_size) {
  for (std::vector<AndroidDeviceManager::BrowserInfo>::const_iterator it =
      device_info.browser_info.begin();
      it != device_info.browser_info.end();
      ++it) {
    browsers_.push_back(new DevToolsAndroidBridge::RemoteBrowser(device, *it));
  }
}

void DevToolsAndroidBridge::RemoteDevice::OpenSocket(
    const std::string& socket_name,
    const AndroidDeviceManager::SocketCallback& callback) {
  device_->OpenSocket(socket_name, callback);
}

DevToolsAndroidBridge::RemoteDevice::~RemoteDevice() {
}

// DevToolsAndroidBridge ------------------------------------------------------

DevToolsAndroidBridge::DevToolsAndroidBridge(Profile* profile)
    : profile_(profile),
      device_manager_(AndroidDeviceManager::Create()),
      task_scheduler_(base::Bind(&DevToolsAndroidBridge::ScheduleTaskDefault)),
      port_forwarding_controller_(new PortForwardingController(profile)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&DevToolsAndroidBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  CreateDeviceProviders();
}

void DevToolsAndroidBridge::AddDeviceListListener(
    DeviceListListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool polling_was_off = !NeedsDeviceListPolling();
  device_list_listeners_.push_back(listener);
  if (polling_was_off)
    StartDeviceListPolling();
}

void DevToolsAndroidBridge::RemoveDeviceListListener(
    DeviceListListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceListListeners::iterator it = std::find(
      device_list_listeners_.begin(), device_list_listeners_.end(), listener);
  DCHECK(it != device_list_listeners_.end());
  device_list_listeners_.erase(it);
  if (!NeedsDeviceListPolling())
    StopDeviceListPolling();
}

void DevToolsAndroidBridge::AddDeviceCountListener(
    DeviceCountListener* listener) {
  device_count_listeners_.push_back(listener);
  if (device_count_listeners_.size() == 1)
    StartDeviceCountPolling();
}

void DevToolsAndroidBridge::RemoveDeviceCountListener(
    DeviceCountListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceCountListeners::iterator it = std::find(
      device_count_listeners_.begin(), device_count_listeners_.end(), listener);
  DCHECK(it != device_count_listeners_.end());
  device_count_listeners_.erase(it);
  if (device_count_listeners_.empty())
    StopDeviceCountPolling();
}

void DevToolsAndroidBridge::AddPortForwardingListener(
    PortForwardingListener* listener) {
  bool polling_was_off = !NeedsDeviceListPolling();
  port_forwarding_listeners_.push_back(listener);
  if (polling_was_off)
    StartDeviceListPolling();
}

void DevToolsAndroidBridge::RemovePortForwardingListener(
    PortForwardingListener* listener) {
  PortForwardingListeners::iterator it = std::find(
      port_forwarding_listeners_.begin(),
      port_forwarding_listeners_.end(),
      listener);
  DCHECK(it != port_forwarding_listeners_.end());
  port_forwarding_listeners_.erase(it);
  if (!NeedsDeviceListPolling())
    StopDeviceListPolling();
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
  DCHECK(port_forwarding_listeners_.empty());
}

void DevToolsAndroidBridge::StartDeviceListPolling() {
  device_list_callback_.Reset(
    base::Bind(&DevToolsAndroidBridge::ReceivedDeviceList, this));
  RequestDeviceList(device_list_callback_.callback());
}

void DevToolsAndroidBridge::StopDeviceListPolling() {
  device_list_callback_.Cancel();
  devices_.clear();
}

bool DevToolsAndroidBridge::NeedsDeviceListPolling() {
  return !device_list_listeners_.empty() || !port_forwarding_listeners_.empty();
}

void DevToolsAndroidBridge::RequestDeviceList(
    const base::Callback<void(const RemoteDevices&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!NeedsDeviceListPolling() ||
      !callback.Equals(device_list_callback_.callback()))
    return;

  new DiscoveryRequest(device_manager_.get(), callback);
}

void DevToolsAndroidBridge::ReceivedDeviceList(const RemoteDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DeviceListListeners copy(device_list_listeners_);
  for (DeviceListListeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->DeviceListChanged(devices);

  DevicesStatus status =
      port_forwarding_controller_->DeviceListChanged(devices);
  PortForwardingListeners forwarding_listeners(port_forwarding_listeners_);
  for (PortForwardingListeners::iterator it = forwarding_listeners.begin();
       it != forwarding_listeners.end(); ++it) {
    (*it)->PortStatusChanged(status);
  }

  if (!NeedsDeviceListPolling())
    return;

  devices_ = devices;

  task_scheduler_.Run(
      base::Bind(&DevToolsAndroidBridge::RequestDeviceList,
                 this, device_list_callback_.callback()));
}

void DevToolsAndroidBridge::StartDeviceCountPolling() {
  device_count_callback_.Reset(
      base::Bind(&DevToolsAndroidBridge::ReceivedDeviceCount, this));
  RequestDeviceCount(device_count_callback_.callback());
}

void DevToolsAndroidBridge::StopDeviceCountPolling() {
  device_count_callback_.Cancel();
}

void DevToolsAndroidBridge::RequestDeviceCount(
    const base::Callback<void(int)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (device_count_listeners_.empty() ||
      !callback.Equals(device_count_callback_.callback()))
    return;

  UsbDeviceProvider::CountDevices(callback);
}

void DevToolsAndroidBridge::ReceivedDeviceCount(int count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DeviceCountListeners copy(device_count_listeners_);
  for (DeviceCountListeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->DeviceCountChanged(count);

  if (device_count_listeners_.empty())
     return;

  task_scheduler_.Run(
      base::Bind(&DevToolsAndroidBridge::RequestDeviceCount,
                 this, device_count_callback_.callback()));
}

// static
void DevToolsAndroidBridge::ScheduleTaskDefault(const base::Closure& task) {
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      task,
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}

void DevToolsAndroidBridge::CreateDeviceProviders() {
  AndroidDeviceManager::DeviceProviders device_providers;
#if defined(DEBUG_DEVTOOLS)
  BrowserListTabContentsProvider::EnableTethering();
  // We cannot rely on command line switch here as we might want to connect
  // to another instance of Chrome. Using hard-coded port number instead.
  const int kDefaultDebuggingPort = 9222;
  device_providers.push_back(new SelfAsDeviceProvider(kDefaultDebuggingPort));
#endif
  device_providers.push_back(new AdbDeviceProvider());

  PrefService* service = profile_->GetPrefs();
  const PrefService::Preference* pref =
      service->FindPreference(prefs::kDevToolsDiscoverUsbDevicesEnabled);
  const base::Value* pref_value = pref->GetValue();

  bool enabled;
  if (pref_value->GetAsBoolean(&enabled) && enabled) {
    device_providers.push_back(new UsbDeviceProvider(profile_));
  }
  device_manager_->SetDeviceProviders(device_providers);
  if (NeedsDeviceListPolling()) {
    StopDeviceListPolling();
    StartDeviceListPolling();
  }
}
