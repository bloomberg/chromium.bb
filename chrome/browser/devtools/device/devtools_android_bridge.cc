// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/devtools_android_bridge.h"

#include <stddef.h>
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/port_forwarding_controller.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/remote_debugging_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/user_metrics.h"
#include "net/base/escape.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/devtools/device/cast_device_provider.h"
#endif

using content::BrowserThread;

namespace {

const char kPageListRequest[] = "/json";
const char kVersionRequest[] = "/json/version";
const char kClosePageRequest[] = "/json/close/%s";
const char kNewPageRequestWithURL[] = "/json/new?%s";
const char kActivatePageRequest[] = "/json/activate/%s";
const char kBrowserTargetSocket[] = "/devtools/browser";
const int kAdbPollingIntervalMs = 1000;

const char kPageReloadCommand[] = "Page.reload";

const char kWebViewSocketPrefix[] = "webview_devtools_remote";

bool BrowserIdFromString(const std::string& browser_id_str,
                         DevToolsAndroidBridge::BrowserId* browser_id) {
  size_t colon_pos = browser_id_str.find(':');
  if (colon_pos == std::string::npos)
    return false;
  browser_id->first = browser_id_str.substr(0, colon_pos);
  browser_id->second = browser_id_str.substr(colon_pos + 1);
  return true;
}

}  // namespace

// DiscoveryRequest -----------------------------------------------------

class DevToolsAndroidBridge::DiscoveryRequest
    : public base::RefCountedThreadSafe<DiscoveryRequest,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  DiscoveryRequest(AndroidDeviceManager* device_manager,
                   const DeviceListCallback& callback);
 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<DiscoveryRequest>;
  virtual ~DiscoveryRequest();

  void ReceivedDevices(const AndroidDeviceManager::Devices& devices);
  void ReceivedDeviceInfo(scoped_refptr<AndroidDeviceManager::Device> device,
                          const AndroidDeviceManager::DeviceInfo& device_info);
  void ReceivedVersion(scoped_refptr<RemoteBrowser>,
                       int result,
                       const std::string& response);
  void ReceivedPages(scoped_refptr<RemoteBrowser>,
                     int result,
                     const std::string& response);

  DeviceListCallback callback_;
  CompleteDevices complete_devices_;
};

DevToolsAndroidBridge::DiscoveryRequest::DiscoveryRequest(
    AndroidDeviceManager* device_manager,
    const DeviceListCallback& callback)
    : callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  device_manager->QueryDevices(
      base::Bind(&DiscoveryRequest::ReceivedDevices, this));
}

DevToolsAndroidBridge::DiscoveryRequest::~DiscoveryRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback_.Run(complete_devices_);
}

void DevToolsAndroidBridge::DiscoveryRequest::ReceivedDevices(
    const AndroidDeviceManager::Devices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& device : devices) {
    device->QueryDeviceInfo(
        base::Bind(&DiscoveryRequest::ReceivedDeviceInfo, this, device));
  }
}

void DevToolsAndroidBridge::DiscoveryRequest::ReceivedDeviceInfo(
    scoped_refptr<AndroidDeviceManager::Device> device,
    const AndroidDeviceManager::DeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<RemoteDevice> remote_device =
      new RemoteDevice(device->serial(), device_info);
  complete_devices_.push_back(std::make_pair(device, remote_device));
  for (RemoteBrowsers::iterator it = remote_device->browsers().begin();
       it != remote_device->browsers().end(); ++it) {
    device->SendJsonRequest(
        (*it)->socket(),
        kVersionRequest,
        base::Bind(&DiscoveryRequest::ReceivedVersion, this, *it));
    device->SendJsonRequest(
        (*it)->socket(),
        kPageListRequest,
        base::Bind(&DiscoveryRequest::ReceivedPages, this, *it));
  }
}

void DevToolsAndroidBridge::DiscoveryRequest::ReceivedVersion(
    scoped_refptr<RemoteBrowser> browser,
    int result,
    const std::string& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result < 0)
    return;
  // Parse version, append to package name if available,
  scoped_ptr<base::Value> value = base::JSONReader::Read(response);
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    std::string browser_name;
    if (dict->GetString("Browser", &browser_name)) {
      std::vector<std::string> parts = base::SplitString(
          browser_name, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (parts.size() == 2)
        browser->version_ = parts[1];
      else
        browser->version_ = browser_name;
    }
    std::string package;
    if (dict->GetString("Android-Package", &package)) {
      browser->display_name_ =
          AndroidDeviceManager::GetBrowserName(browser->socket(), package);
    }
  }
}

void DevToolsAndroidBridge::DiscoveryRequest::ReceivedPages(
    scoped_refptr<RemoteBrowser> browser,
    int result,
    const std::string& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result < 0)
    return;
  scoped_ptr<base::Value> value = base::JSONReader::Read(response);
  base::ListValue* list_value;
  if (value && value->GetAsList(&list_value)) {
    for (const auto& page_value : *list_value) {
      base::DictionaryValue* dict;
      if (page_value->GetAsDictionary(&dict))
        browser->pages_.push_back(new RemotePage(browser->browser_id_, *dict));
    }
  }
}

// ProtocolCommand ------------------------------------------------------------

namespace {

class ProtocolCommand
    : public AndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  ProtocolCommand(
      scoped_refptr<AndroidDeviceManager::Device> device,
      const std::string& socket,
      const std::string& target_path,
      const std::string& command,
      const base::Closure callback);

 private:
  void OnSocketOpened() override;
  void OnFrameRead(const std::string& message) override;
  void OnSocketClosed() override;
  ~ProtocolCommand() override;

  const std::string command_;
  const base::Closure callback_;
  scoped_ptr<AndroidDeviceManager::AndroidWebSocket> web_socket_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolCommand);
};

ProtocolCommand::ProtocolCommand(
    scoped_refptr<AndroidDeviceManager::Device> device,
    const std::string& socket,
    const std::string& target_path,
    const std::string& command,
    const base::Closure callback)
    : command_(command),
      callback_(callback),
      web_socket_(device->CreateWebSocket(socket, target_path, this)) {
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

// static
DevToolsAndroidBridge::Factory* DevToolsAndroidBridge::Factory::GetInstance() {
  return base::Singleton<DevToolsAndroidBridge::Factory>::get();
}

// static
DevToolsAndroidBridge* DevToolsAndroidBridge::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<DevToolsAndroidBridge*>(GetInstance()->
          GetServiceForBrowserContext(profile->GetOriginalProfile(), true));
}

DevToolsAndroidBridge::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "DevToolsAndroidBridge",
          BrowserContextDependencyManager::GetInstance()) {
}

DevToolsAndroidBridge::Factory::~Factory() {}

KeyedService* DevToolsAndroidBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new DevToolsAndroidBridge(profile);
}

// AgentHostDelegate ----------------------------------------------------------

class DevToolsAndroidBridge::AgentHostDelegate
    : public content::DevToolsExternalAgentProxyDelegate,
      public AndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  static scoped_refptr<content::DevToolsAgentHost> GetOrCreateAgentHost(
      DevToolsAndroidBridge* bridge,
      const std::string& id,
      const BrowserId& browser_id,
      const std::string& target_path);

 private:
  AgentHostDelegate(
      DevToolsAndroidBridge* bridge,
      const std::string& id,
      const BrowserId& browser_id,
      const std::string& target_path);
  ~AgentHostDelegate() override;
  void Attach(content::DevToolsExternalAgentProxy* proxy) override;
  void Detach() override;
  void SendMessageToBackend(const std::string& message) override;
  void OnSocketOpened() override;
  void OnFrameRead(const std::string& message) override;
  void OnSocketClosed() override;

  std::string id_;
  base::WeakPtr<DevToolsAndroidBridge> bridge_;
  BrowserId browser_id_;
  std::string target_path_;
  bool socket_opened_;
  std::vector<std::string> pending_messages_;
  scoped_refptr<AndroidDeviceManager::Device> device_;
  scoped_ptr<AndroidDeviceManager::AndroidWebSocket> web_socket_;
  content::DevToolsAgentHost* agent_host_;
  content::DevToolsExternalAgentProxy* proxy_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

// static
scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::AgentHostDelegate::GetOrCreateAgentHost(
    DevToolsAndroidBridge* bridge,
    const std::string& id,
    const BrowserId& browser_id,
    const std::string& target_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AgentHostDelegates::iterator it = bridge->host_delegates_.find(id);
  if (it != bridge->host_delegates_.end())
    return it->second->agent_host_;

  AgentHostDelegate* delegate =
      new AgentHostDelegate(bridge, id, browser_id, target_path);
  scoped_refptr<content::DevToolsAgentHost> result =
      content::DevToolsAgentHost::Create(delegate);
  delegate->agent_host_ = result.get();
  return result;
}

DevToolsAndroidBridge::AgentHostDelegate::AgentHostDelegate(
    DevToolsAndroidBridge* bridge,
    const std::string& id,
    const BrowserId& browser_id,
    const std::string& target_path)
    : id_(id),
      bridge_(bridge->AsWeakPtr()),
      browser_id_(browser_id),
      target_path_(target_path),
      socket_opened_(false),
      agent_host_(NULL),
      proxy_(NULL) {
  bridge_->host_delegates_[id] = this;
}

DevToolsAndroidBridge::AgentHostDelegate::~AgentHostDelegate() {
  if (bridge_)
    bridge_->host_delegates_.erase(id_);
}

void DevToolsAndroidBridge::AgentHostDelegate::Attach(
    content::DevToolsExternalAgentProxy* proxy) {
  proxy_ = proxy;
  content::RecordAction(browser_id_.second.find(kWebViewSocketPrefix) == 0 ?
      base::UserMetricsAction("DevTools_InspectAndroidWebView") :
      base::UserMetricsAction("DevTools_InspectAndroidPage"));

  // Retain the device so it's not released until AgentHost is detached.
  if (bridge_)
    device_ = bridge_->FindDevice(browser_id_.first);
  if (!device_.get())
    return;

  web_socket_.reset(
      device_->CreateWebSocket(browser_id_.second, target_path_, this));
}

void DevToolsAndroidBridge::AgentHostDelegate::Detach() {
  web_socket_.reset();
  device_ = nullptr;
  proxy_ = nullptr;
}

void DevToolsAndroidBridge::AgentHostDelegate::SendMessageToBackend(
    const std::string& message) {
  // We could have detached due to physical connection being closed.
  if (!proxy_)
    return;
  if (socket_opened_)
    web_socket_->SendFrame(message);
  else
    pending_messages_.push_back(message);
}

void DevToolsAndroidBridge::AgentHostDelegate::OnSocketOpened() {
  socket_opened_ = true;
  for (std::vector<std::string>::iterator it = pending_messages_.begin();
       it != pending_messages_.end(); ++it) {
    SendMessageToBackend(*it);
  }
  pending_messages_.clear();
}

void DevToolsAndroidBridge::AgentHostDelegate::OnFrameRead(
    const std::string& message) {
  if (proxy_)
      proxy_->DispatchOnClientHost(message);
}

void DevToolsAndroidBridge::AgentHostDelegate::OnSocketClosed() {
  if (proxy_) {
    std::string message = "{ \"method\": \"Inspector.detached\", "
        "\"params\": { \"reason\": \"Connection lost.\"} }";
    proxy_->DispatchOnClientHost(message);
    Detach();
  }
}

//// RemotePageTarget ----------------------------------------------

class DevToolsAndroidBridge::RemotePageTarget : public DevToolsTargetImpl {
 public:
  RemotePageTarget(DevToolsAndroidBridge* bridge,
                   const BrowserId& browser_id,
                   const base::DictionaryValue& value);
  ~RemotePageTarget() override;

  // DevToolsTargetImpl overrides.
  std::string GetId() const override;
  bool IsAttached() const override;
  bool Activate() const override;
  bool Close() const override;
  void Inspect(Profile* profile) const override;
  void Reload() const override;

 private:
  base::WeakPtr<DevToolsAndroidBridge> bridge_;
  BrowserId browser_id_;
  std::string target_path_;
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
    const DevToolsAndroidBridge::BrowserId& browser_id,
    const base::DictionaryValue& value) {
  return base::StringPrintf("%s:%s:%s", browser_id.first.c_str(),
      browser_id.second.c_str(), GetStringProperty(value, "id").c_str());
}

static std::string GetFrontendURL(const base::DictionaryValue& value) {
  std::string frontend_url = GetStringProperty(value, "devtoolsFrontendUrl");
  size_t ws_param = frontend_url.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url = frontend_url.substr(0, ws_param);
  if (frontend_url.find("http:") == 0)
    frontend_url = "https:" + frontend_url.substr(5);
  return frontend_url;
}

static std::string GetTargetPath(const base::DictionaryValue& value) {
  std::string target_path = GetStringProperty(value, "webSocketDebuggerUrl");

  if (target_path.find("ws://") == 0) {
    size_t pos = target_path.find("/", 5);
    if (pos == std::string::npos)
      pos = 5;
    target_path = target_path.substr(pos);
  } else {
    target_path = std::string();
  }
  return target_path;
}

DevToolsAndroidBridge::RemotePageTarget::RemotePageTarget(
    DevToolsAndroidBridge* bridge,
    const BrowserId& browser_id,
    const base::DictionaryValue& value)
    : DevToolsTargetImpl(AgentHostDelegate::GetOrCreateAgentHost(
                             bridge,
                             BuildUniqueTargetId(browser_id, value),
                             browser_id,
                             GetTargetPath(value))),
      bridge_(bridge->AsWeakPtr()),
      browser_id_(browser_id),
      target_path_(GetTargetPath(value)),
      frontend_url_(GetFrontendURL(value)),
      remote_id_(GetStringProperty(value, "id")),
      remote_type_(GetStringProperty(value, "type")),
      local_id_(BuildUniqueTargetId(browser_id, value)) {
  set_type("adb_page");
  set_url(GURL(GetStringProperty(value, "url")));
  set_title(base::UTF16ToUTF8(net::UnescapeForHTML(base::UTF8ToUTF16(
      GetStringProperty(value, "title")))));
  set_description(GetStringProperty(value, "description"));
  set_favicon_url(GURL(GetStringProperty(value, "faviconUrl")));
  target_path_ = GetTargetPath(value);
}

DevToolsAndroidBridge::RemotePageTarget::~RemotePageTarget() {
}

std::string DevToolsAndroidBridge::RemotePageTarget::GetId() const {
  return local_id_;
}

bool DevToolsAndroidBridge::RemotePageTarget::IsAttached() const {
  return target_path_.empty();
}

static void NoOp(int, const std::string&) {}

void DevToolsAndroidBridge::RemotePageTarget::Inspect(Profile* profile) const {
  Activate();
  bool isWorker = remote_type_ == kTargetTypeWorker ||
                  remote_type_ == kTargetTypeServiceWorker;
  DevToolsWindow::OpenExternalFrontend(profile, frontend_url_, GetAgentHost(),
                                       isWorker);
}

bool DevToolsAndroidBridge::RemotePageTarget::Activate() const {
  if (!bridge_)
    return false;

  std::string request = base::StringPrintf(kActivatePageRequest,
                                           remote_id_.c_str());
  bridge_->SendJsonRequest(browser_id_, request, base::Bind(&NoOp));
  return true;
}

bool DevToolsAndroidBridge::RemotePageTarget::Close() const {
  if (!bridge_)
    return false;

  std::string request = base::StringPrintf(kClosePageRequest,
                                           remote_id_.c_str());
  bridge_->SendJsonRequest(browser_id_, request, base::Bind(&NoOp));
  return true;
}

void DevToolsAndroidBridge::RemotePageTarget::Reload() const {
  if (!bridge_)
    return;

  bridge_->SendProtocolCommand(browser_id_, target_path_, kPageReloadCommand,
                               NULL, base::Closure());
}

// DevToolsAndroidBridge::RemotePage ------------------------------------------

DevToolsAndroidBridge::RemotePage::RemotePage(const BrowserId& browser_id,
                                              const base::DictionaryValue& dict)
    : browser_id_(browser_id),
      frontend_url_(GetFrontendURL(dict)),
      dict_(dict.DeepCopy()) {
}

DevToolsAndroidBridge::RemotePage::~RemotePage() {
}

// DevToolsAndroidBridge::RemoteBrowser ---------------------------------------

DevToolsAndroidBridge::RemoteBrowser::RemoteBrowser(
    const std::string& serial,
    const AndroidDeviceManager::BrowserInfo& browser_info)
    : browser_id_(std::make_pair(serial, browser_info.socket_name)),
      display_name_(browser_info.display_name),
      user_(browser_info.user),
      type_(browser_info.type) {
}

bool DevToolsAndroidBridge::RemoteBrowser::IsChrome() {
  return type_ == AndroidDeviceManager::BrowserInfo::kTypeChrome;
}

std::string DevToolsAndroidBridge::RemoteBrowser::GetId() {
  return serial() + ":" + socket();
}

DevToolsAndroidBridge::RemoteBrowser::ParsedVersion
DevToolsAndroidBridge::RemoteBrowser::GetParsedVersion() {
  ParsedVersion result;
  for (const base::StringPiece& part :
       base::SplitStringPiece(
           version_, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    int value = 0;
    base::StringToInt(part, &value);
    result.push_back(value);
  }
  return result;
}

DevToolsTargetImpl*
DevToolsAndroidBridge::CreatePageTarget(scoped_refptr<RemotePage> page) {
  return new RemotePageTarget(this, page->browser_id_, *page->dict_);
}

void DevToolsAndroidBridge::SendJsonRequest(
    const BrowserId& browser_id,
    const std::string& request,
    const JsonRequestCallback& callback) {
  scoped_refptr<AndroidDeviceManager::Device> device(
      FindDevice(browser_id.first));
  if (!device.get()) {
    callback.Run(net::ERR_FAILED, std::string());
    return;
  }
  device->SendJsonRequest(browser_id.second, request, callback);
}

void DevToolsAndroidBridge::SendProtocolCommand(
    const BrowserId& browser_id,
    const std::string& target_path,
    const std::string& method,
    scoped_ptr<base::DictionaryValue> params,
    const base::Closure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (target_path.empty())
    return;
  scoped_refptr<AndroidDeviceManager::Device> device(
      FindDevice(browser_id.first));
  if (!device.get()) {
    callback.Run();
    return;
  }
  new ProtocolCommand(
      device, browser_id.second, target_path,
      DevToolsProtocol::SerializeCommand(1, method, std::move(params)),
      callback);
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::GetBrowserAgentHost(
    scoped_refptr<RemoteBrowser> browser) {
  return AgentHostDelegate::GetOrCreateAgentHost(
      this,
      "adb:" + browser->serial() + ":" + browser->socket(),
      browser->browser_id_,
      kBrowserTargetSocket);
}

void DevToolsAndroidBridge::SendJsonRequest(
    const std::string& browser_id_str,
    const std::string& url,
    const JsonRequestCallback& callback) {
  BrowserId browser_id;
  if (!BrowserIdFromString(browser_id_str, &browser_id)) {
    callback.Run(net::ERR_FAILED, std::string());
    return;
  }
  SendJsonRequest(browser_id, url, callback);
}

scoped_refptr<AndroidDeviceManager::Device> DevToolsAndroidBridge::FindDevice(
    const std::string& serial) {
  DeviceMap::iterator it = device_map_.find(serial);
  return it == device_map_.end() ? nullptr : it->second;
}

void DevToolsAndroidBridge::OpenRemotePage(scoped_refptr<RemoteBrowser> browser,
                                           const std::string& input_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL gurl(input_url);
  if (!gurl.is_valid()) {
    gurl = GURL("http://" + input_url);
    if (!gurl.is_valid())
      return;
  }
  std::string url = gurl.spec();
  RemoteBrowser::ParsedVersion parsed_version = browser->GetParsedVersion();

  std::string query = net::EscapeQueryParamValue(url, false /* use_plus */);
  std::string request =
      base::StringPrintf(kNewPageRequestWithURL, query.c_str());
  SendJsonRequest(browser->browser_id_, request, base::Bind(&NoOp));
}

DevToolsAndroidBridge::RemoteBrowser::~RemoteBrowser() {
}

// DevToolsAndroidBridge::RemoteDevice ----------------------------------------

DevToolsAndroidBridge::RemoteDevice::RemoteDevice(
    const std::string& serial,
    const AndroidDeviceManager::DeviceInfo& device_info)
    : serial_(serial),
      model_(device_info.model),
      connected_(device_info.connected),
      screen_size_(device_info.screen_size) {
  for (std::vector<AndroidDeviceManager::BrowserInfo>::const_iterator it =
      device_info.browser_info.begin();
      it != device_info.browser_info.end();
      ++it) {
    browsers_.push_back(new RemoteBrowser(serial, *it));
  }
}

DevToolsAndroidBridge::RemoteDevice::~RemoteDevice() {
}

// DevToolsAndroidBridge ------------------------------------------------------

DevToolsAndroidBridge::DevToolsAndroidBridge(
    Profile* profile)
    : profile_(profile),
      device_manager_(AndroidDeviceManager::Create()),
      task_scheduler_(base::Bind(&DevToolsAndroidBridge::ScheduleTaskDefault)),
      port_forwarding_controller_(new PortForwardingController(profile, this)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&DevToolsAndroidBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  CreateDeviceProviders();
}

void DevToolsAndroidBridge::AddDeviceListListener(
    DeviceListListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool polling_was_off = !NeedsDeviceListPolling();
  device_list_listeners_.push_back(listener);
  if (polling_was_off)
    StartDeviceListPolling();
}

void DevToolsAndroidBridge::RemoveDeviceListListener(
    DeviceListListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

bool DevToolsAndroidBridge::HasDevToolsWindow(const std::string& agent_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return host_delegates_.find(agent_id) != host_delegates_.end();
}

DevToolsAndroidBridge::~DevToolsAndroidBridge() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(device_list_listeners_.empty());
  DCHECK(device_count_listeners_.empty());
  DCHECK(port_forwarding_listeners_.empty());
}

void DevToolsAndroidBridge::StartDeviceListPolling() {
  device_list_callback_.Reset(
      base::Bind(&DevToolsAndroidBridge::ReceivedDeviceList, AsWeakPtr()));
  RequestDeviceList(device_list_callback_.callback());
}

void DevToolsAndroidBridge::StopDeviceListPolling() {
  device_list_callback_.Cancel();
  device_map_.clear();
}

bool DevToolsAndroidBridge::NeedsDeviceListPolling() {
  return !device_list_listeners_.empty() || !port_forwarding_listeners_.empty();
}

void DevToolsAndroidBridge::RequestDeviceList(
    const DeviceListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!NeedsDeviceListPolling() ||
      !callback.Equals(device_list_callback_.callback()))
    return;

  new DiscoveryRequest(device_manager_.get(), callback);
}

void DevToolsAndroidBridge::ReceivedDeviceList(
    const CompleteDevices& complete_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device_map_.clear();
  RemoteDevices remote_devices;
  for (const auto& pair : complete_devices) {
    device_map_[pair.first->serial()] = pair.first;
    remote_devices.push_back(pair.second);
  }

  DeviceListListeners copy(device_list_listeners_);
  for (DeviceListListeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->DeviceListChanged(remote_devices);

  ForwardingStatus status =
      port_forwarding_controller_->DeviceListChanged(remote_devices);
  PortForwardingListeners forwarding_listeners(port_forwarding_listeners_);
  for (PortForwardingListeners::iterator it = forwarding_listeners.begin();
       it != forwarding_listeners.end(); ++it) {
    (*it)->PortStatusChanged(status);
  }

  if (!NeedsDeviceListPolling())
    return;

  task_scheduler_.Run(
      base::Bind(&DevToolsAndroidBridge::RequestDeviceList,
                 AsWeakPtr(), device_list_callback_.callback()));
}

void DevToolsAndroidBridge::StartDeviceCountPolling() {
  device_count_callback_.Reset(
      base::Bind(&DevToolsAndroidBridge::ReceivedDeviceCount, AsWeakPtr()));
  RequestDeviceCount(device_count_callback_.callback());
}

void DevToolsAndroidBridge::StopDeviceCountPolling() {
  device_count_callback_.Cancel();
}

void DevToolsAndroidBridge::RequestDeviceCount(
    const base::Callback<void(int)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_count_listeners_.empty() ||
      !callback.Equals(device_count_callback_.callback()))
    return;

  UsbDeviceProvider::CountDevices(callback);
}

void DevToolsAndroidBridge::ReceivedDeviceCount(int count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DeviceCountListeners copy(device_count_listeners_);
  for (DeviceCountListeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->DeviceCountChanged(count);

  if (device_count_listeners_.empty())
     return;

  task_scheduler_.Run(
      base::Bind(&DevToolsAndroidBridge::RequestDeviceCount,
                 AsWeakPtr(), device_count_callback_.callback()));
}

// static
void DevToolsAndroidBridge::ScheduleTaskDefault(const base::Closure& task) {
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      task,
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}

static scoped_refptr<TCPDeviceProvider> CreateTCPDeviceProvider() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kRemoteDebuggingTargets))
    return nullptr;
  std::string value =
      command_line->GetSwitchValueASCII(switches::kRemoteDebuggingTargets);
  std::vector<std::string> addresses = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::set<net::HostPortPair> targets;
  for (const std::string& address : addresses) {
    net::HostPortPair target = net::HostPortPair::FromString(address);
    if (target.IsEmpty()) {
      LOG(WARNING) << "Invalid target: " << address;
      continue;
    }
    targets.insert(target);
  }
  if (targets.empty())
    return nullptr;
  return new TCPDeviceProvider(targets);
}

void DevToolsAndroidBridge::CreateDeviceProviders() {
  AndroidDeviceManager::DeviceProviders device_providers;

  if (scoped_refptr<TCPDeviceProvider> provider = CreateTCPDeviceProvider())
    device_providers.push_back(provider);

#if defined(ENABLE_SERVICE_DISCOVERY)
  device_providers.push_back(new CastDeviceProvider());
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
