// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"

#include <map>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb/android_rsa.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/devtools/adb_web_socket.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kListProcessesCommand[] = "shell:ps";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kDumpsysScreenSizePrefix[] = "mStable=";

const char kUnknownModel[] = "Offline";

const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
const char kVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";
const char kClosePageRequest[] = "GET /json/close/%s HTTP/1.1\r\n\r\n";
const char kNewPageRequest[] = "GET /json/new HTTP/1.1\r\n\r\n";
const char kActivatePageRequest[] =
    "GET /json/activate/%s HTTP/1.1\r\n\r\n";
const int kAdbPollingIntervalMs = 1000;

const char kUrlParam[] = "url";
const char kPageReloadCommand[] = "Page.reload";
const char kPageNavigateCommand[] = "Page.navigate";

#if defined(DEBUG_DEVTOOLS)
const char kChrome[] = "Chrome";
const char kLocalChrome[] = "Local Chrome";
#endif  // defined(DEBUG_DEVTOOLS)

typedef DevToolsAdbBridge::Callback Callback;
typedef std::vector<scoped_refptr<AndroidDevice> >
    AndroidDevices;
typedef base::Callback<void(const AndroidDevices&)> AndroidDevicesCallback;

// AdbPagesCommand ------------------------------------------------------------

class AdbPagesCommand : public base::RefCountedThreadSafe<
    AdbPagesCommand,
    BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(DevToolsAdbBridge::RemoteDevices*)> Callback;

  AdbPagesCommand(
      scoped_refptr<RefCountedAdbThread> adb_thread,
      const DevToolsAdbBridge::DeviceProviders& device_providers,
      const Callback& callback);

 private:
  friend struct BrowserThread::DeleteOnThread<
      BrowserThread::UI>;
  friend class base::DeleteHelper<AdbPagesCommand>;

  virtual ~AdbPagesCommand();
  void ProcessDeviceProviders();
  void ReceivedDevices(const AndroidDevices& devices);

  void ProcessSerials();
  void ReceivedModel(int result, const std::string& response);
  void ReceivedSockets(int result, const std::string& response);
  void ReceivedDumpsys(int result, const std::string& response);
  void ReceivedProcesses(int result, const std::string& response);
  void ProcessSockets();
  void ReceivedVersion(int result, const std::string& response);
  void ReceivedPages(int result, const std::string& response);
  void Respond();
  void ParseSocketsList(const std::string& response);
  void ParseProcessList(const std::string& response);
  void ParseDumpsysResponse(const std::string& response);
  void ParseScreenSize(const std::string& str);

  scoped_refptr<RefCountedAdbThread> adb_thread_;
  Callback callback_;
  AndroidDevices devices_;
  DevToolsAdbBridge::RemoteBrowsers browsers_;
  scoped_ptr<DevToolsAdbBridge::RemoteDevices> remote_devices_;
  DevToolsAdbBridge::DeviceProviders device_providers_;
};

AdbPagesCommand::AdbPagesCommand(
    scoped_refptr<RefCountedAdbThread> adb_thread,
    const DevToolsAdbBridge::DeviceProviders& device_providers,
    const Callback& callback)
    : adb_thread_(adb_thread),
      callback_(callback),
      device_providers_(device_providers){
  remote_devices_.reset(new DevToolsAdbBridge::RemoteDevices());

  ProcessDeviceProviders();
}

AdbPagesCommand::~AdbPagesCommand() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void AdbPagesCommand::ProcessDeviceProviders() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (device_providers_.empty()) {
    adb_thread_->message_loop()->PostTask(
              FROM_HERE, base::Bind(&AdbPagesCommand::ProcessSerials, this));
    return;
  }

  const scoped_refptr<AndroidDeviceProvider>& device_provider =
      device_providers_.back();

  device_provider->QueryDevices(
      base::Bind(&AdbPagesCommand::ReceivedDevices, this));
}

void AdbPagesCommand::ReceivedDevices(const AndroidDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!device_providers_.empty());
  device_providers_.pop_back();

  devices_.insert(devices_.end(), devices.begin(), devices.end());

  if (!device_providers_.empty()) {
    ProcessDeviceProviders();
  } else {
    adb_thread_->message_loop()->PostTask(
          FROM_HERE, base::Bind(&AdbPagesCommand::ProcessSerials, this));
  }
}

void AdbPagesCommand::ProcessSerials() {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (devices_.size() == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbPagesCommand::Respond, this));
    return;
  }

#if defined(DEBUG_DEVTOOLS)
  // For desktop remote debugging.
  if (devices_.back()->serial().empty()) {
    scoped_refptr<AndroidDevice> device =
        devices_.back();
    device->set_model(kLocalChrome);
    remote_devices_->push_back(
        new DevToolsAdbBridge::RemoteDevice(device));
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> remote_browser =
        new DevToolsAdbBridge::RemoteBrowser(
            adb_thread_, device, std::string());
    remote_browser->set_product(kChrome);
    remote_devices_->back()->AddBrowser(remote_browser);
    browsers_.push_back(remote_browser);
    device->HttpQuery(
        std::string(), kVersionRequest,
        base::Bind(&AdbPagesCommand::ReceivedVersion, this));
    return;
  }
#endif  // defined(DEBUG_DEVTOOLS)

  scoped_refptr<AndroidDevice> device = devices_.back();
  if (device->is_connected()) {
    device->RunCommand(kDeviceModelCommand,
                       base::Bind(&AdbPagesCommand::ReceivedModel, this));
  } else {
    device->set_model(kUnknownModel);
    remote_devices_->push_back(new DevToolsAdbBridge::RemoteDevice(device));
    devices_.pop_back();
    ProcessSerials();
  }
}

void AdbPagesCommand::ReceivedModel(int result, const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result < 0) {
    devices_.pop_back();
    ProcessSerials();
    return;
  }
  scoped_refptr<AndroidDevice> device = devices_.back();
  device->set_model(response);
  remote_devices_->push_back(
      new DevToolsAdbBridge::RemoteDevice(device));
  device->RunCommand(kOpenedUnixSocketsCommand,
                     base::Bind(&AdbPagesCommand::ReceivedSockets, this));
}

void AdbPagesCommand::ReceivedSockets(int result,
                                      const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result < 0) {
    devices_.pop_back();
    ProcessSerials();
    return;
  }

  ParseSocketsList(response);
  scoped_refptr<AndroidDevice> device = devices_.back();
  device->RunCommand(kDumpsysCommand,
                     base::Bind(&AdbPagesCommand::ReceivedDumpsys, this));
}

void AdbPagesCommand::ReceivedDumpsys(int result,
                                      const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result >= 0)
    ParseDumpsysResponse(response);

  scoped_refptr<AndroidDevice> device = devices_.back();
  device->RunCommand(kListProcessesCommand,
                     base::Bind(&AdbPagesCommand::ReceivedProcesses, this));
}

void AdbPagesCommand::ReceivedProcesses(int result,
                                        const std::string& response) {
  if (result >= 0)
    ParseProcessList(response);

  if (browsers_.size() == 0) {
    devices_.pop_back();
    ProcessSerials();
  } else {
    ProcessSockets();
  }
}

void AdbPagesCommand::ProcessSockets() {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (browsers_.size() == 0) {
    devices_.pop_back();
    ProcessSerials();
  } else {
    scoped_refptr<AndroidDevice> device = devices_.back();
    device->HttpQuery(browsers_.back()->socket(), kVersionRequest,
                      base::Bind(&AdbPagesCommand::ReceivedVersion, this));
  }
}

void AdbPagesCommand::ReceivedVersion(int result,
                     const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result < 0) {
    browsers_.pop_back();
    ProcessSockets();
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
      if (parts.size() == 2) {
        if (parts[0] != "Version")  // WebView has this for legacy reasons.
          browsers_.back()->set_product(parts[0]);
        browsers_.back()->set_version(parts[1]);
      } else {
        browsers_.back()->set_version(browser);
      }
    }
  }

  scoped_refptr<AndroidDevice> device = devices_.back();
  device->HttpQuery(browsers_.back()->socket(), kPageListRequest,
                    base::Bind(&AdbPagesCommand::ReceivedPages, this));
}

void AdbPagesCommand::ReceivedPages(int result,
                   const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser = browsers_.back();
  browsers_.pop_back();
  if (result < 0) {
    ProcessSockets();
    return;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::ListValue* list_value;
  if (!value || !value->GetAsList(&list_value)) {
    ProcessSockets();
    return;
  }

  base::Value* item;

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    list_value->Get(i, &item);
    base::DictionaryValue* dict;
    if (!item || !item->GetAsDictionary(&dict))
      continue;
    browser->AddPage(new DevToolsAdbBridge::RemotePage(
        adb_thread_, browser->device(), browser->socket(), *dict));
  }
  ProcessSockets();
}

void AdbPagesCommand::Respond() {
  callback_.Run(remote_devices_.release());
}

void AdbPagesCommand::ParseSocketsList(const std::string& response) {
  // On Android, '/proc/net/unix' looks like this:
  //
  // Num       RefCount Protocol Flags    Type St Inode Path
  // 00000000: 00000002 00000000 00010000 0001 01 331813 /dev/socket/zygote
  // 00000000: 00000002 00000000 00010000 0001 01 358606 @xxx_devtools_remote
  // 00000000: 00000002 00000000 00010000 0001 01 347300 @yyy_devtools_remote
  //
  // We need to find records with paths starting from '@' (abstract socket)
  // and containing "devtools_remote". We have to extract the inode number
  // in order to find the owning process name.

  scoped_refptr<DevToolsAdbBridge::RemoteDevice> remote_device =
      remote_devices_->back();

  std::vector<std::string> entries;
  Tokenize(response, "\n", &entries);
  const std::string channel_pattern =
      base::StringPrintf(kDevToolsChannelNameFormat, "");
  for (size_t i = 1; i < entries.size(); ++i) {
    std::vector<std::string> fields;
    Tokenize(entries[i], " \r", &fields);
    if (fields.size() < 8)
      continue;
    if (fields[3] != "00010000" || fields[5] != "01")
      continue;
    std::string path_field = fields[7];
    if (path_field.size() < 1 || path_field[0] != '@')
      continue;
    size_t socket_name_pos = path_field.find(channel_pattern);
    if (socket_name_pos == std::string::npos)
      continue;

    std::string socket = path_field.substr(1);
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> remote_browser =
        new DevToolsAdbBridge::RemoteBrowser(
            adb_thread_, remote_device->device(), socket);

    std::string product = path_field.substr(1, socket_name_pos - 1);
    product[0] = base::ToUpperASCII(product[0]);
    remote_browser->set_product(product);

    size_t socket_name_end = socket_name_pos + channel_pattern.size();
    if (socket_name_end < path_field.size() &&
        path_field[socket_name_end] == '_') {
      remote_browser->set_pid(path_field.substr(socket_name_end + 1));
    }
    remote_device->AddBrowser(remote_browser);
  }
  browsers_ = remote_device->browsers();
}

void AdbPagesCommand::ParseProcessList(const std::string& response) {
  // On Android, 'ps' output looks like this:
  // USER PID PPID VSIZE RSS WCHAN PC ? NAME
  typedef std::map<std::string, std::string> StringMap;
  StringMap pid_to_package;
  std::vector<std::string> entries;
  Tokenize(response, "\n", &entries);
  for (size_t i = 1; i < entries.size(); ++i) {
    std::vector<std::string> fields;
    Tokenize(entries[i], " \r", &fields);
    if (fields.size() < 9)
      continue;
    pid_to_package[fields[1]] = fields[8];
  }
  DevToolsAdbBridge::RemoteBrowsers browsers =
      remote_devices_->back()->browsers();
  for (DevToolsAdbBridge::RemoteBrowsers::iterator it = browsers.begin();
      it != browsers.end(); ++it) {
    StringMap::iterator pit = pid_to_package.find((*it)->pid());
    if (pit != pid_to_package.end())
      (*it)->set_package(pit->second);
  }
}

void AdbPagesCommand::ParseDumpsysResponse(const std::string& response) {
  std::vector<std::string> lines;
  Tokenize(response, "\r", &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];
    size_t pos = line.find(kDumpsysScreenSizePrefix);
    if (pos != std::string::npos) {
      ParseScreenSize(
          line.substr(pos + std::string(kDumpsysScreenSizePrefix).size()));
      break;
    }
  }
}

void AdbPagesCommand::ParseScreenSize(const std::string& str) {
  std::vector<std::string> pairs;
  Tokenize(str, "-", &pairs);
  if (pairs.size() != 2)
    return;

  int width;
  int height;
  std::vector<std::string> numbers;
  Tokenize(pairs[1].substr(1, pairs[1].size() - 2), ",", &numbers);
  if (numbers.size() != 2 ||
      !base::StringToInt(numbers[0], &width) ||
      !base::StringToInt(numbers[1], &height))
    return;

  remote_devices_->back()->set_screen_size(gfx::Size(width, height));
}


// AdbProtocolCommand ---------------------------------------------------------

class AdbProtocolCommand : public AdbWebSocket::Delegate {
 public:
  AdbProtocolCommand(
      scoped_refptr<RefCountedAdbThread> adb_thread,
      scoped_refptr<AndroidDevice> device,
      const std::string& socket_name,
      const std::string& debug_url,
      const std::string& command);

 private:
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;
  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE;

  scoped_refptr<RefCountedAdbThread> adb_thread_;
  const std::string command_;
  scoped_refptr<AdbWebSocket> web_socket_;

  DISALLOW_COPY_AND_ASSIGN(AdbProtocolCommand);
};

AdbProtocolCommand::AdbProtocolCommand(
    scoped_refptr<RefCountedAdbThread> adb_thread,
    scoped_refptr<AndroidDevice> device,
    const std::string& socket_name,
    const std::string& debug_url,
    const std::string& command)
    : adb_thread_(adb_thread),
      command_(command) {
  web_socket_ = new AdbWebSocket(
      device, socket_name, debug_url, adb_thread_->message_loop(), this);
}

void AdbProtocolCommand::OnSocketOpened() {
  web_socket_->SendFrame(command_);
  web_socket_->Disconnect();
}

void AdbProtocolCommand::OnFrameRead(const std::string& message) {}

void AdbProtocolCommand::OnSocketClosed(bool closed_by_device) {
  delete this;
}

bool AdbProtocolCommand::ProcessIncomingMessage(const std::string& message) {
  return false;
}

}  // namespace

const char kDevToolsChannelNameFormat[] = "%s_devtools_remote";

class AgentHostDelegate;

typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;

base::LazyInstance<AgentHostDelegates>::Leaky g_host_delegates =
    LAZY_INSTANCE_INITIALIZER;

DevToolsAdbBridge::Wrapper::Wrapper() {
  bridge_ = new DevToolsAdbBridge();
}

DevToolsAdbBridge::Wrapper::~Wrapper() {
}

DevToolsAdbBridge* DevToolsAdbBridge::Wrapper::Get() {
  return bridge_.get();
}

// static
DevToolsAdbBridge::Factory* DevToolsAdbBridge::Factory::GetInstance() {
  return Singleton<DevToolsAdbBridge::Factory>::get();
}

// static
DevToolsAdbBridge* DevToolsAdbBridge::Factory::GetForProfile(
    Profile* profile) {
  DevToolsAdbBridge::Wrapper* wrapper =
      static_cast<DevToolsAdbBridge::Wrapper*>(GetInstance()->
          GetServiceForBrowserContext(profile, true));
  return wrapper ? wrapper->Get() : NULL;
}

DevToolsAdbBridge::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "DevToolsAdbBridge",
          BrowserContextDependencyManager::GetInstance()) {}

DevToolsAdbBridge::Factory::~Factory() {}

BrowserContextKeyedService*
DevToolsAdbBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevToolsAdbBridge::Wrapper();
}


// AgentHostDelegate ----------------------------------------------------------

class AgentHostDelegate : public content::DevToolsExternalAgentProxyDelegate,
                          public AdbWebSocket::Delegate {
 public:
  AgentHostDelegate(
      const std::string& id,
      scoped_refptr<AndroidDevice> device,
      const std::string& socket_name,
      const std::string& debug_url,
      const std::string& frontend_url,
      base::MessageLoop* adb_message_loop,
      Profile* profile)
      : id_(id),
        serial_(device->serial()),
        frontend_url_(frontend_url),
        adb_message_loop_(adb_message_loop),
        profile_(profile) {
    web_socket_ = new AdbWebSocket(
        device, socket_name, debug_url, adb_message_loop, this);
    g_host_delegates.Get()[id] = this;
  }

  void OpenFrontend() {
    if (!proxy_)
      return;
    DevToolsWindow::OpenExternalFrontend(
        profile_, frontend_url_, proxy_->GetAgentHost().get());
  }

 private:
  virtual ~AgentHostDelegate() {
    g_host_delegates.Get().erase(id_);
  }

  virtual void Attach() OVERRIDE {}

  virtual void Detach() OVERRIDE {
    web_socket_->Disconnect();
  }

  virtual void SendMessageToBackend(const std::string& message) OVERRIDE {
    web_socket_->SendFrame(message);
  }

  virtual void OnSocketOpened() OVERRIDE {
    proxy_.reset(content::DevToolsExternalAgentProxy::Create(this));
    OpenFrontend();
  }

  virtual void OnFrameRead(const std::string& message) OVERRIDE {
    proxy_->DispatchOnClientHost(message);
  }

  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE {
    if (proxy_ && closed_by_device)
      proxy_->ConnectionClosed();
    delete this;
  }

  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE {
    return false;
  }

  const std::string id_;
  const std::string serial_;
  const std::string frontend_url_;
  base::MessageLoop* adb_message_loop_;
  Profile* profile_;

  scoped_ptr<content::DevToolsExternalAgentProxy> proxy_;
  scoped_refptr<AdbWebSocket> web_socket_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};


// DevToolsAdbBridge::RemotePage ----------------------------------------------

DevToolsAdbBridge::RemotePage::RemotePage(
    scoped_refptr<RefCountedAdbThread> adb_thread,
    scoped_refptr<AndroidDevice> device,
    const std::string& socket,
    const base::DictionaryValue& value)
    : adb_thread_(adb_thread),
      device_(device),
      socket_(socket) {
  value.GetString("id", &id_);
  value.GetString("url", &url_);
  value.GetString("title", &title_);
  value.GetString("description", &description_);
  value.GetString("faviconUrl", &favicon_url_);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
  value.GetString("devtoolsFrontendUrl", &frontend_url_);

  if (id_.empty() && !debug_url_.empty())  {
    // Target id is not available until Chrome 26. Use page id at the end of
    // debug_url_ instead. For attached targets the id will remain empty.
    std::vector<std::string> parts;
    Tokenize(debug_url_, "/", &parts);
    id_ = parts[parts.size()-1];
  }

  if (debug_url_.find("ws://") == 0)
    debug_url_ = debug_url_.substr(5);
  else
    debug_url_ = "";

  size_t ws_param = frontend_url_.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url_ = frontend_url_.substr(0, ws_param);
  if (frontend_url_.find("http:") == 0)
    frontend_url_ = "https:" + frontend_url_.substr(5);

  agent_id_ = base::StringPrintf("%s:%s:%s",
      device_->serial().c_str(), socket_.c_str(), id_.c_str());
}

bool DevToolsAdbBridge::RemotePage::HasDevToolsWindow() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_host_delegates.Get().find(agent_id_) != g_host_delegates.Get().end();
}

void DevToolsAdbBridge::RemotePage::Inspect(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RequestActivate(
      base::Bind(&RemotePage::InspectOnHandlerThread, this, profile));
}

static void Noop(int, const std::string&) {}

void DevToolsAdbBridge::RemotePage::Activate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RequestActivate(base::Bind(&Noop));
}

void DevToolsAdbBridge::RemotePage::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (attached())
    return;
  std::string request = base::StringPrintf(kClosePageRequest, id_.c_str());
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDevice::HttpQuery,
          device_, socket_, request, base::Bind(&Noop)));
}

void DevToolsAdbBridge::RemotePage::Reload() {
  SendProtocolCommand(kPageReloadCommand, NULL);
}

void DevToolsAdbBridge::RemotePage::SendProtocolCommand(
    const std::string& method,
    base::DictionaryValue* params) {
  if (attached())
    return;
  DevToolsProtocol::Command command(1, method, params);
  new AdbProtocolCommand(
      adb_thread_, device_, socket_, debug_url_, command.Serialize());
}

DevToolsAdbBridge::RemotePage::~RemotePage() {
}

void DevToolsAdbBridge::RemotePage::RequestActivate(
    const AndroidDevice::CommandCallback& callback) {
  std::string request = base::StringPrintf(kActivatePageRequest, id_.c_str());
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDevice::HttpQuery,
          device_, socket_, request, callback));
}

void DevToolsAdbBridge::RemotePage::InspectOnHandlerThread(
    Profile* profile, int result, const std::string& response) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemotePage::InspectOnUIThread, this, profile));
}

void DevToolsAdbBridge::RemotePage::InspectOnUIThread(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AgentHostDelegates::iterator it =
      g_host_delegates.Get().find(agent_id_);
  if (it != g_host_delegates.Get().end()) {
    it->second->OpenFrontend();
  } else if (!attached()) {
    new AgentHostDelegate(
        agent_id_, device_, socket_, debug_url_,
        frontend_url_, adb_thread_->message_loop(), profile);
  }
}


// DevToolsAdbBridge::RemoteBrowser -------------------------------------------

DevToolsAdbBridge::RemoteBrowser::RemoteBrowser(
    scoped_refptr<RefCountedAdbThread> adb_thread,
    scoped_refptr<AndroidDevice> device,
    const std::string& socket)
    : adb_thread_(adb_thread),
      device_(device),
      socket_(socket) {
}

void DevToolsAdbBridge::RemoteBrowser::Open(const std::string& url) {
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDevice::HttpQuery,
          device_, socket_, kNewPageRequest,
          base::Bind(&RemoteBrowser::PageCreatedOnHandlerThread, this, url)));
}

void DevToolsAdbBridge::RemoteBrowser::PageCreatedOnHandlerThread(
    const std::string& url, int result, const std::string& response) {
  if (result < 0)
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemoteBrowser::PageCreatedOnUIThread, this, response, url));
}

void DevToolsAdbBridge::RemoteBrowser::PageCreatedOnUIThread(
    const std::string& response, const std::string& url) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    scoped_refptr<RemotePage> new_page =
        new RemotePage(adb_thread_, device_, socket_, *dict);
    base::DictionaryValue params;
    params.SetString(kUrlParam, url);
    new_page->SendProtocolCommand(kPageNavigateCommand, &params);
  }
}

DevToolsAdbBridge::RemoteBrowser::~RemoteBrowser() {
}


// DevToolsAdbBridge::RemoteDevice --------------------------------------------

DevToolsAdbBridge::RemoteDevice::RemoteDevice(
    scoped_refptr<AndroidDevice> device)
    : device_(device) {
}

std::string DevToolsAdbBridge::RemoteDevice::GetSerial() {
  return device_->serial();
}

std::string DevToolsAdbBridge::RemoteDevice::GetModel() {
  return device_->model();
}

bool DevToolsAdbBridge::RemoteDevice::IsConnected() {
  return device_->is_connected();
}

void DevToolsAdbBridge::RemoteDevice::AddBrowser(
    scoped_refptr<RemoteBrowser> browser) {
  browsers_.push_back(browser);
}

DevToolsAdbBridge::RemoteDevice::~RemoteDevice() {
}


// DevToolsAdbBridge ----------------------------------------------------------

DevToolsAdbBridge::DevToolsAdbBridge()
    : adb_thread_(RefCountedAdbThread::GetInstance()),
      has_message_loop_(adb_thread_->message_loop() != NULL) {
}

void DevToolsAdbBridge::AddListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (listeners_.empty())
    RequestRemoteDevices();
  listeners_.push_back(listener);
}

void DevToolsAdbBridge::RemoveListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Listeners::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(listeners_.empty());
}

void DevToolsAdbBridge::RequestRemoteDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  new AdbPagesCommand(
      adb_thread_, device_providers_,
      base::Bind(&DevToolsAdbBridge::ReceivedRemoteDevices, this));
}

void DevToolsAdbBridge::ReceivedRemoteDevices(RemoteDevices* devices_ptr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<RemoteDevices> devices(devices_ptr);

  Listeners copy(listeners_);
  for (Listeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->RemoteDevicesChanged(devices.get());

  if (listeners_.empty())
    return;

  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsAdbBridge::RequestRemoteDevices, this),
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}
