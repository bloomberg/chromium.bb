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
#include "base/memory/ptr_util.h"
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
using content::DevToolsAgentHost;

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

const char kChromeDiscoveryURL[] = "localhost:9222";
const char kNodeDiscoveryURL[] = "localhost:9229";

bool BrowserIdFromString(const std::string& browser_id_str,
                         std::string* serial,
                         std::string* browser_id) {
  size_t colon_pos = browser_id_str.find(':');
  if (colon_pos == std::string::npos)
    return false;
  *serial = browser_id_str.substr(0, colon_pos);
  *browser_id = browser_id_str.substr(colon_pos + 1);
  return true;
}

static void NoOp(int, const std::string&) {}

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
  void ReceivedPages(scoped_refptr<AndroidDeviceManager::Device> device,
                     scoped_refptr<RemoteBrowser>,
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
        base::Bind(&DiscoveryRequest::ReceivedPages, this, device, *it));
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
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response);
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
    scoped_refptr<AndroidDeviceManager::Device> device,
    scoped_refptr<RemoteBrowser> browser,
    int result,
    const std::string& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result < 0)
    return;
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response);
  base::ListValue* list_value;
  if (value && value->GetAsList(&list_value)) {
    for (const auto& page_value : *list_value) {
      base::DictionaryValue* dict;
      if (page_value->GetAsDictionary(&dict))
        browser->pages_.push_back(
            new RemotePage(device, browser->browser_id_, *dict));
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
  std::unique_ptr<AndroidDeviceManager::AndroidWebSocket> web_socket_;

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
      scoped_refptr<AndroidDeviceManager::Device> device,
      const std::string& browser_id,
      const std::string& local_id,
      const std::string& target_path,
      const std::string& type,
      base::DictionaryValue* value);
  ~AgentHostDelegate() override;

 private:
  AgentHostDelegate(
      scoped_refptr<AndroidDeviceManager::Device> device,
      const std::string& browser_id,
      const std::string& local_id,
      const std::string& target_path,
      const std::string& type,
      base::DictionaryValue* value);
  // DevToolsExternalAgentProxyDelegate overrides.
  void Attach(content::DevToolsExternalAgentProxy* proxy) override;
  void Detach() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  void SendMessageToBackend(const std::string& message) override;

  void OnSocketOpened() override;
  void OnFrameRead(const std::string& message) override;
  void OnSocketClosed() override;

  void SendProtocolCommand(const std::string& target_path,
                           const std::string& method,
                           std::unique_ptr<base::DictionaryValue> params,
                           const base::Closure callback);

  scoped_refptr<AndroidDeviceManager::Device> device_;
  std::string browser_id_;
  std::string local_id_;
  std::string target_path_;
  std::string remote_type_;
  std::string remote_id_;
  std::string frontend_url_;
  std::string title_;
  std::string description_;
  GURL url_;
  GURL favicon_url_;
  bool socket_opened_;
  std::vector<std::string> pending_messages_;
  std::unique_ptr<AndroidDeviceManager::AndroidWebSocket> web_socket_;
  content::DevToolsAgentHost* agent_host_;
  content::DevToolsExternalAgentProxy* proxy_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

static std::string GetStringProperty(base::DictionaryValue* value,
                                     const std::string& name) {
  std::string result;
  value->GetString(name, &result);
  return result;
}

static std::string BuildUniqueTargetId(
    const std::string& serial,
    const std::string& browser_id,
    base::DictionaryValue* value) {
  return base::StringPrintf("%s:%s:%s", serial.c_str(),
      browser_id.c_str(), GetStringProperty(value, "id").c_str());
}

static std::string GetFrontendURLFromValue(base::DictionaryValue* value) {
  std::string frontend_url = GetStringProperty(value, "devtoolsFrontendUrl");
  size_t ws_param = frontend_url.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url = frontend_url.substr(0, ws_param);
  if (base::StartsWith(frontend_url, "http:", base::CompareCase::SENSITIVE))
    frontend_url = "https:" + frontend_url.substr(5);
  return frontend_url;
}

static std::string GetTargetPath(base::DictionaryValue* value) {
  std::string target_path = GetStringProperty(value, "webSocketDebuggerUrl");

  if (base::StartsWith(target_path, "ws://", base::CompareCase::SENSITIVE)) {
    size_t pos = target_path.find("/", 5);
    if (pos == std::string::npos)
      pos = 5;
    target_path = target_path.substr(pos);
  } else {
    target_path = std::string();
  }
  return target_path;
}

// static
scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::AgentHostDelegate::GetOrCreateAgentHost(
    scoped_refptr<AndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const std::string& local_id,
    const std::string& target_path,
    const std::string& type,
    base::DictionaryValue* value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<DevToolsAgentHost> result =
      DevToolsAgentHost::GetForId(local_id);
  if (result)
    return result;

  AgentHostDelegate* delegate = new AgentHostDelegate(
      device, browser_id, local_id, target_path, type, value);
  result = content::DevToolsAgentHost::Forward(
      local_id, base::WrapUnique(delegate));
  delegate->agent_host_ = result.get();
  return result;
}

DevToolsAndroidBridge::AgentHostDelegate::AgentHostDelegate(
    scoped_refptr<AndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const std::string& local_id,
    const std::string& target_path,
    const std::string& type,
    base::DictionaryValue* value)
    : device_(device),
      browser_id_(browser_id),
      local_id_(local_id),
      target_path_(target_path),
      remote_type_(type),
      remote_id_(value ? GetStringProperty(value, "id") : ""),
      frontend_url_(value ? GetFrontendURLFromValue(value) : ""),
      title_(value ? base::UTF16ToUTF8(net::UnescapeForHTML(base::UTF8ToUTF16(
          GetStringProperty(value, "title")))) : ""),
      description_(value ? GetStringProperty(value, "description") : ""),
      url_(GURL(value ? GetStringProperty(value, "url") : "")),
      favicon_url_(GURL(value ? GetStringProperty(value, "faviconUrl") : "")),
      socket_opened_(false),
      agent_host_(nullptr),
      proxy_(nullptr) {
}

DevToolsAndroidBridge::AgentHostDelegate::~AgentHostDelegate() {
}

void DevToolsAndroidBridge::AgentHostDelegate::Attach(
    content::DevToolsExternalAgentProxy* proxy) {
  proxy_ = proxy;
  content::RecordAction(
      base::StartsWith(browser_id_, kWebViewSocketPrefix,
                       base::CompareCase::SENSITIVE)
          ? base::UserMetricsAction("DevTools_InspectAndroidWebView")
          : base::UserMetricsAction("DevTools_InspectAndroidPage"));
  web_socket_.reset(
      device_->CreateWebSocket(browser_id_, target_path_, this));
}

void DevToolsAndroidBridge::AgentHostDelegate::Detach() {
  web_socket_.reset();
  proxy_ = nullptr;
}

std::string DevToolsAndroidBridge::AgentHostDelegate::GetType() {
  return remote_type_;
}

std::string DevToolsAndroidBridge::AgentHostDelegate::GetTitle() {
  return title_;
}

std::string DevToolsAndroidBridge::AgentHostDelegate::GetDescription() {
  return description_;
}

GURL DevToolsAndroidBridge::AgentHostDelegate::GetURL() {
  return url_;
}

GURL DevToolsAndroidBridge::AgentHostDelegate::GetFaviconURL() {
  return favicon_url_;
}

std::string DevToolsAndroidBridge::AgentHostDelegate::GetFrontendURL() {
  return frontend_url_;
}

bool DevToolsAndroidBridge::AgentHostDelegate::Activate() {
  std::string request = base::StringPrintf(kActivatePageRequest,
                                           remote_id_.c_str());
  device_->SendJsonRequest(browser_id_, request, base::Bind(&NoOp));
  return true;
}

void DevToolsAndroidBridge::AgentHostDelegate::Reload() {
  SendProtocolCommand(target_path_, kPageReloadCommand, nullptr,
                      base::Closure());
}

bool DevToolsAndroidBridge::AgentHostDelegate::Close() {
  std::string request = base::StringPrintf(kClosePageRequest,
                                           remote_id_.c_str());
  device_->SendJsonRequest(browser_id_, request, base::Bind(&NoOp));
  return true;
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

void DevToolsAndroidBridge::AgentHostDelegate::SendProtocolCommand(
    const std::string& target_path,
    const std::string& method,
    std::unique_ptr<base::DictionaryValue> params,
    const base::Closure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (target_path.empty())
    return;
  new ProtocolCommand(
      device_, browser_id_, target_path,
      DevToolsProtocol::SerializeCommand(1, method, std::move(params)),
      callback);
}

// DevToolsAndroidBridge::RemotePage ------------------------------------------

DevToolsAndroidBridge::RemotePage::RemotePage(
    scoped_refptr<AndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const base::DictionaryValue& dict)
    : device_(device),
      browser_id_(browser_id),
      dict_(dict.DeepCopy()) {
}

DevToolsAndroidBridge::RemotePage::~RemotePage() {
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::RemotePage::CreateTarget() {
  std::string local_id = BuildUniqueTargetId(device_->serial(),
                                             browser_id_,
                                             dict_.get());
  std::string target_path = GetTargetPath(dict_.get());
  std::string type = GetStringProperty(dict_.get(), "type");

  return AgentHostDelegate::GetOrCreateAgentHost(
      device_, browser_id_, local_id, target_path, type, dict_.get());
}

// DevToolsAndroidBridge::RemoteBrowser ---------------------------------------

DevToolsAndroidBridge::RemoteBrowser::RemoteBrowser(
    const std::string& serial,
    const AndroidDeviceManager::BrowserInfo& browser_info)
    : serial_(serial),
      browser_id_(browser_info.socket_name),
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

scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::GetBrowserAgentHost(
    scoped_refptr<RemoteBrowser> browser) {
  DeviceMap::iterator it = device_map_.find(browser->serial());
  if (it == device_map_.end())
    return nullptr;

  return AgentHostDelegate::GetOrCreateAgentHost(
      it->second,
      browser->browser_id_,
      "adb:" + browser->serial() + ":" + browser->socket(),
      kBrowserTargetSocket, DevToolsAgentHost::kTypeBrowser, nullptr);
}

void DevToolsAndroidBridge::SendJsonRequest(
    const std::string& browser_id_str,
    const std::string& url,
    const JsonRequestCallback& callback) {
  std::string serial;
  std::string browser_id;
  if (!BrowserIdFromString(browser_id_str, &serial, &browser_id)) {
    callback.Run(net::ERR_FAILED, std::string());
    return;
  }
  DeviceMap::iterator it = device_map_.find(serial);
  if (it == device_map_.end()) {
    callback.Run(net::ERR_FAILED, std::string());
    return;
  }
  it->second->SendJsonRequest(browser_id, url, callback);
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
  SendJsonRequest(browser->GetId(), request, base::Bind(&NoOp));
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

// static
void DevToolsAndroidBridge::QueryCompleteDevices(
    AndroidDeviceManager* device_manager,
    const DeviceListCallback& callback) {
  new DiscoveryRequest(device_manager, callback);
}

DevToolsAndroidBridge::DevToolsAndroidBridge(
    Profile* profile)
    : profile_(profile),
      device_manager_(AndroidDeviceManager::Create()),
      task_scheduler_(base::Bind(&DevToolsAndroidBridge::ScheduleTaskDefault)),
      port_forwarding_controller_(new PortForwardingController(profile)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&DevToolsAndroidBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsTCPDiscoveryConfig,
      base::Bind(&DevToolsAndroidBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverTCPTargetsEnabled,
      base::Bind(&DevToolsAndroidBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  base::ListValue* target_discovery = new base::ListValue();
  target_discovery->AppendString(kChromeDiscoveryURL);
  target_discovery->AppendString(kNodeDiscoveryURL);
  profile->GetPrefs()->SetDefaultPrefValue(
      prefs::kDevToolsTCPDiscoveryConfig, target_discovery);
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
  port_forwarding_controller_->CloseAllConnections();
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
      port_forwarding_controller_->DeviceListChanged(complete_devices);
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

static std::set<net::HostPortPair> ParseTargetDiscoveryPreferenceValue(
    const base::ListValue* preferenceValue) {
  std::set<net::HostPortPair> targets;
  if (!preferenceValue || preferenceValue->empty())
    return targets;
  std::string address;
  for (size_t i = 0; i < preferenceValue->GetSize(); i++) {
    if (!preferenceValue->GetString(i, &address))
      continue;
    net::HostPortPair target = net::HostPortPair::FromString(address);
    if (target.IsEmpty()) {
      LOG(WARNING) << "Invalid target: " << address;
      continue;
    }
    targets.insert(target);
  }
  return targets;
}

static scoped_refptr<TCPDeviceProvider> CreateTCPDeviceProvider(
    const base::ListValue* targetDiscoveryConfig) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::set<net::HostPortPair> targets =
      ParseTargetDiscoveryPreferenceValue(targetDiscoveryConfig);
  if (targets.empty() &&
      !command_line->HasSwitch(switches::kRemoteDebuggingTargets))
    return nullptr;
  std::string value =
      command_line->GetSwitchValueASCII(switches::kRemoteDebuggingTargets);
  std::vector<std::string> addresses = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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
  PrefService* service = profile_->GetPrefs();
  const base::ListValue* targets =
      service->GetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled)
          ? service->GetList(prefs::kDevToolsTCPDiscoveryConfig)
          : nullptr;
  scoped_refptr<TCPDeviceProvider> provider = CreateTCPDeviceProvider(targets);
  if (tcp_provider_callback_)
    tcp_provider_callback_.Run(provider);

  if (provider)
    device_providers.push_back(provider);

#if defined(ENABLE_SERVICE_DISCOVERY)
  device_providers.push_back(new CastDeviceProvider());
#endif

  device_providers.push_back(new AdbDeviceProvider());

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

void DevToolsAndroidBridge::set_tcp_provider_callback_for_test(
    TCPProviderCallback callback) {
  tcp_provider_callback_ = callback;
  CreateDeviceProviders();
}
