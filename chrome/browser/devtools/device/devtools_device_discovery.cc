// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/devtools_device_discovery.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "net/base/escape.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using RemoteBrowser = DevToolsDeviceDiscovery::RemoteBrowser;
using RemoteDevice = DevToolsDeviceDiscovery::RemoteDevice;
using RemotePage = DevToolsDeviceDiscovery::RemotePage;

namespace {

const char kPageListRequest[] = "/json";
const char kVersionRequest[] = "/json/version";
const char kClosePageRequest[] = "/json/close/%s";
const char kActivatePageRequest[] = "/json/activate/%s";
const char kBrowserTargetSocket[] = "/devtools/browser";
const int kPollingIntervalMs = 1000;

const char kPageReloadCommand[] = "Page.reload";

const char kWebViewSocketPrefix[] = "webview_devtools_remote";

static void NoOp(int, const std::string&) {}

static void ScheduleTaskDefault(const base::Closure& task) {
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      task,
      base::TimeDelta::FromMilliseconds(kPollingIntervalMs));
}

// ProtocolCommand ------------------------------------------------------------

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

// AgentHostDelegate ----------------------------------------------------------

class AgentHostDelegate
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
  base::TimeTicks GetLastActivityTime() override;
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
AgentHostDelegate::GetOrCreateAgentHost(
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

AgentHostDelegate::AgentHostDelegate(
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

AgentHostDelegate::~AgentHostDelegate() {
}

void AgentHostDelegate::Attach(content::DevToolsExternalAgentProxy* proxy) {
  proxy_ = proxy;
  base::RecordAction(
      base::StartsWith(browser_id_, kWebViewSocketPrefix,
                       base::CompareCase::SENSITIVE)
          ? base::UserMetricsAction("DevTools_InspectAndroidWebView")
          : base::UserMetricsAction("DevTools_InspectAndroidPage"));
  web_socket_.reset(
      device_->CreateWebSocket(browser_id_, target_path_, this));
}

void AgentHostDelegate::Detach() {
  web_socket_.reset();
  proxy_ = nullptr;
}

std::string AgentHostDelegate::GetType() {
  return remote_type_;
}

std::string AgentHostDelegate::GetTitle() {
  return title_;
}

std::string AgentHostDelegate::GetDescription() {
  return description_;
}

GURL AgentHostDelegate::GetURL() {
  return url_;
}

GURL AgentHostDelegate::GetFaviconURL() {
  return favicon_url_;
}

std::string AgentHostDelegate::GetFrontendURL() {
  return frontend_url_;
}

bool AgentHostDelegate::Activate() {
  std::string request = base::StringPrintf(kActivatePageRequest,
                                           remote_id_.c_str());
  device_->SendJsonRequest(browser_id_, request, base::Bind(&NoOp));
  return true;
}

void AgentHostDelegate::Reload() {
  SendProtocolCommand(target_path_, kPageReloadCommand, nullptr,
                      base::Closure());
}

bool AgentHostDelegate::Close() {
  std::string request = base::StringPrintf(kClosePageRequest,
                                           remote_id_.c_str());
  device_->SendJsonRequest(browser_id_, request, base::Bind(&NoOp));
  return true;
}

base::TimeTicks AgentHostDelegate::GetLastActivityTime() {
  return base::TimeTicks();
}

void AgentHostDelegate::SendMessageToBackend(const std::string& message) {
  // We could have detached due to physical connection being closed.
  if (!proxy_)
    return;
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
  content::DevToolsExternalAgentProxy* proxy = proxy_;
  if (proxy) {
    std::string message = "{ \"method\": \"Inspector.detached\", "
        "\"params\": { \"reason\": \"Connection lost.\"} }";
    proxy->DispatchOnClientHost(message);
    Detach();
    proxy->ConnectionClosed();  // May delete |this|.
 }
}

void AgentHostDelegate::SendProtocolCommand(
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

}  // namespace

// DevToolsDeviceDiscovery::DiscoveryRequest ----------------------------------

class DevToolsDeviceDiscovery::DiscoveryRequest
    : public base::RefCountedThreadSafe<DiscoveryRequest,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  DiscoveryRequest(AndroidDeviceManager* device_manager,
                   const DevToolsDeviceDiscovery::DeviceListCallback& callback);
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

  DevToolsDeviceDiscovery::DeviceListCallback callback_;
  DevToolsDeviceDiscovery::CompleteDevices complete_devices_;
};

DevToolsDeviceDiscovery::DiscoveryRequest::DiscoveryRequest(
    AndroidDeviceManager* device_manager,
    const DevToolsDeviceDiscovery::DeviceListCallback& callback)
    : callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  device_manager->QueryDevices(
      base::Bind(&DiscoveryRequest::ReceivedDevices, this));
}

DevToolsDeviceDiscovery::DiscoveryRequest::~DiscoveryRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback_.Run(complete_devices_);
}

void DevToolsDeviceDiscovery::DiscoveryRequest::ReceivedDevices(
    const AndroidDeviceManager::Devices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& device : devices) {
    device->QueryDeviceInfo(
        base::Bind(&DiscoveryRequest::ReceivedDeviceInfo, this, device));
  }
}

void DevToolsDeviceDiscovery::DiscoveryRequest::ReceivedDeviceInfo(
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

void DevToolsDeviceDiscovery::DiscoveryRequest::ReceivedVersion(
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
    browser->browser_target_id_ = GetTargetPath(dict);
    if (browser->browser_target_id_.empty())
      browser->browser_target_id_ = kBrowserTargetSocket;
    std::string package;
    if (dict->GetString("Android-Package", &package)) {
      browser->display_name_ =
          AndroidDeviceManager::GetBrowserName(browser->socket(), package);
    }
  }
}

void DevToolsDeviceDiscovery::DiscoveryRequest::ReceivedPages(
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
      const base::DictionaryValue* dict;
      if (page_value.GetAsDictionary(&dict))
        browser->pages_.push_back(
            new RemotePage(device, browser->browser_id_, *dict));
    }
  }
}

// DevToolsDeviceDiscovery::RemotePage ----------------------------------------

DevToolsDeviceDiscovery::RemotePage::RemotePage(
    scoped_refptr<AndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const base::DictionaryValue& dict)
    : device_(device),
      browser_id_(browser_id),
      dict_(dict.DeepCopy()) {
}

DevToolsDeviceDiscovery::RemotePage::~RemotePage() {
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsDeviceDiscovery::RemotePage::CreateTarget() {
  std::string local_id = BuildUniqueTargetId(device_->serial(),
                                             browser_id_,
                                             dict_.get());
  std::string target_path = GetTargetPath(dict_.get());
  std::string type = GetStringProperty(dict_.get(), "type");
  agent_host_ = AgentHostDelegate::GetOrCreateAgentHost(
      device_, browser_id_, local_id, target_path, type, dict_.get());
  return agent_host_;
}

// DevToolsDeviceDiscovery::RemoteBrowser -------------------------------------

DevToolsDeviceDiscovery::RemoteBrowser::RemoteBrowser(
    const std::string& serial,
    const AndroidDeviceManager::BrowserInfo& browser_info)
    : serial_(serial),
      browser_id_(browser_info.socket_name),
      display_name_(browser_info.display_name),
      user_(browser_info.user),
      type_(browser_info.type) {
}

bool DevToolsDeviceDiscovery::RemoteBrowser::IsChrome() {
  return type_ == AndroidDeviceManager::BrowserInfo::kTypeChrome;
}

std::string DevToolsDeviceDiscovery::RemoteBrowser::GetId() {
  return serial() + ":" + socket();
}

DevToolsDeviceDiscovery::RemoteBrowser::ParsedVersion
DevToolsDeviceDiscovery::RemoteBrowser::GetParsedVersion() {
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

DevToolsDeviceDiscovery::RemoteBrowser::~RemoteBrowser() {
}

// DevToolsDeviceDiscovery::RemoteDevice --------------------------------------

DevToolsDeviceDiscovery::RemoteDevice::RemoteDevice(
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

DevToolsDeviceDiscovery::RemoteDevice::~RemoteDevice() {
}

// DevToolsDeviceDiscovery ----------------------------------------------------

DevToolsDeviceDiscovery::DevToolsDeviceDiscovery(
    AndroidDeviceManager* device_manager,
    const DeviceListCallback& callback)
    : device_manager_(device_manager),
      callback_(callback),
      task_scheduler_(base::Bind(&ScheduleTaskDefault)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RequestDeviceList();
}

DevToolsDeviceDiscovery::~DevToolsDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void DevToolsDeviceDiscovery::SetScheduler(
    base::Callback<void(const base::Closure&)> scheduler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_scheduler_ = scheduler;
}

// static
scoped_refptr<content::DevToolsAgentHost>
DevToolsDeviceDiscovery::CreateBrowserAgentHost(
    scoped_refptr<AndroidDeviceManager::Device> device,
    scoped_refptr<RemoteBrowser> browser) {
  return AgentHostDelegate::GetOrCreateAgentHost(
      device, browser->browser_id_,
      "adb:" + browser->serial() + ":" + browser->socket(),
      browser->browser_target_id(), DevToolsAgentHost::kTypeBrowser, nullptr);
}

void DevToolsDeviceDiscovery::RequestDeviceList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  new DiscoveryRequest(
      device_manager_,
      base::Bind(&DevToolsDeviceDiscovery::ReceivedDeviceList,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsDeviceDiscovery::ReceivedDeviceList(
    const CompleteDevices& complete_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_scheduler_.Run(base::Bind(&DevToolsDeviceDiscovery::RequestDeviceList,
                                 weak_factory_.GetWeakPtr()));
  // |callback_| should be run last as it may destroy |this|.
  callback_.Run(complete_devices);
}
