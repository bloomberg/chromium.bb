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
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb/android_rsa.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/user_metrics.h"
#include "crypto/rsa_private_key.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kInstalledChromePackagesCommand[] = "shell:pm list packages";
const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kListProcessesCommand[] = "shell:ps";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kDumpsysScreenSizePrefix[] = "mStable=";

const char kUnknownModel[] = "Offline";

const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
const char kVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";
const char kClosePageRequest[] = "GET /json/close/%s HTTP/1.1\r\n\r\n";
const char kNewPageRequest[] = "GET /json/new HTTP/1.1\r\n\r\n";
const char kNewPageRequestWithURL[] = "GET /json/new?%s HTTP/1.1\r\n\r\n";
const char kActivatePageRequest[] =
    "GET /json/activate/%s HTTP/1.1\r\n\r\n";
const int kAdbPollingIntervalMs = 1000;

const char kUrlParam[] = "url";
const char kPageReloadCommand[] = "Page.reload";
const char kPageNavigateCommand[] = "Page.navigate";

const char kChromeDefaultName[] = "Chrome";
const char kChromeDefaultSocket[] = "chrome_devtools_remote";
const int kMinVersionNewWithURL = 32;
const int kNewPageNavigateDelayMs = 500;

const char kWebViewSocketPrefix[] = "webview_devtools_remote";
const char kWebViewNameTemplate[] = "WebView in %s";

struct BrowserDescriptor {
  const char* package;
  const char* socket;
  const char* display_name;
};

const BrowserDescriptor kBrowserDescriptors[] = {
  {
    "com.android.chrome",
    kChromeDefaultSocket,
    kChromeDefaultName
  },
  {
    "com.chrome.beta",
    kChromeDefaultSocket,
    "Chrome Beta"
  },
  {
    "com.google.android.apps.chrome_dev",
    kChromeDefaultSocket,
    "Chrome Dev"
  },
  {
    "com.chrome.canary",
    kChromeDefaultSocket,
    "Chrome Canary"
  },
  {
    "com.google.android.apps.chrome",
    kChromeDefaultSocket,
    "Chromium"
  },
  {
    "org.chromium.content_shell_apk",
    "content_shell_devtools_remote",
    "Content Shell"
  },
  {
    "org.chromium.chrome.shell",
    "chrome_shell_devtools_remote",
    "Chrome Shell"
  },
  {
    "org.chromium.android_webview.shell",
    "webview_devtools_remote",
    "WebView Test Shell"
  }
};

const BrowserDescriptor* FindBrowserDescriptor(const std::string& package) {
  int count = sizeof(kBrowserDescriptors) / sizeof(kBrowserDescriptors[0]);
  for (int i = 0; i < count; i++)
    if (kBrowserDescriptors[i].package == package)
      return &kBrowserDescriptors[i];
  return NULL;
}

typedef std::map<std::string, const BrowserDescriptor*> DescriptorMap;

static DescriptorMap FindInstalledBrowserPackages(
    const std::string& response) {
  // Parse 'pm list packages' output which on Android looks like this:
  //
  // package:com.android.chrome
  // package:com.chrome.beta
  // package:com.example.app
  //
  DescriptorMap package_to_descriptor;
  const std::string package_prefix = "package:";
  std::vector<std::string> entries;
  Tokenize(response, "'\r\n", &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].find(package_prefix) != 0)
      continue;
    std::string package = entries[i].substr(package_prefix.size());
    const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
    if (!descriptor)
      continue;
    package_to_descriptor[descriptor->package] = descriptor;
  }
  return package_to_descriptor;
}

typedef std::map<std::string, std::string> StringMap;

static void MapProcessesToPackages(const std::string& response,
                                   StringMap& pid_to_package,
                                   StringMap& package_to_pid) {
  // Parse 'ps' output which on Android looks like this:
  //
  // USER PID PPID VSIZE RSS WCHAN PC ? NAME
  //
  std::vector<std::string> entries;
  Tokenize(response, "\n", &entries);
  for (size_t i = 1; i < entries.size(); ++i) {
    std::vector<std::string> fields;
    Tokenize(entries[i], " \r", &fields);
    if (fields.size() < 9)
      continue;
    std::string pid = fields[1];
    std::string package = fields[8];
    pid_to_package[pid] = package;
    package_to_pid[package] = pid;
  }
}

typedef std::map<std::string,
                 scoped_refptr<DevToolsAdbBridge::RemoteBrowser> > BrowserMap;

static StringMap MapSocketsToProcesses(const std::string& response,
                                       const std::string& channel_pattern) {
  // Parse 'cat /proc/net/unix' output which on Android looks like this:
  //
  // Num       RefCount Protocol Flags    Type St Inode Path
  // 00000000: 00000002 00000000 00010000 0001 01 331813 /dev/socket/zygote
  // 00000000: 00000002 00000000 00010000 0001 01 358606 @xxx_devtools_remote
  // 00000000: 00000002 00000000 00010000 0001 01 347300 @yyy_devtools_remote
  //
  // We need to find records with paths starting from '@' (abstract socket)
  // and containing the channel pattern ("_devtools_remote").
  StringMap socket_to_pid;
  std::vector<std::string> entries;
  Tokenize(response, "\n", &entries);
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

    std::string pid;
    size_t socket_name_end = socket_name_pos + channel_pattern.size();
    if (socket_name_end < path_field.size() &&
        path_field[socket_name_end] == '_') {
      pid = path_field.substr(socket_name_end + 1);
    }
    socket_to_pid[socket] = pid;
  }
  return socket_to_pid;
}

// AdbPagesCommand ------------------------------------------------------------

class AdbPagesCommand : public base::RefCountedThreadSafe<
    AdbPagesCommand,
    BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(DevToolsAdbBridge::RemoteDevices*)> Callback;

  AdbPagesCommand(
      scoped_refptr<DevToolsAdbBridge> adb_bridge,
      AndroidDeviceManager* device_manager,
      base::MessageLoop* device_message_loop,
      const DevToolsAdbBridge::DeviceProviders& device_providers,
      const Callback& callback);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<AdbPagesCommand>;

  virtual ~AdbPagesCommand();

  void ReceivedSerials(const std::vector<std::string>& serials);
  void ProcessSerials();
  void ReceivedModel(int result, const std::string& response);
  void ReceivedDumpsys(int result, const std::string& response);
  void ReceivedPackages(int result, const std::string& response);
  void ReceivedProcesses(
      const std::string& packages_response,
      int result,
      const std::string& processes_response);
  void ReceivedSockets(
      const std::string& packages_response,
      const std::string& processes_response,
      int result,
      const std::string& sockets_response);
  void ProcessSockets();
  void ReceivedVersion(int result, const std::string& response);
  void ReceivedPages(int result, const std::string& response);

  std::string current_serial() const { return serials_.back(); }

  scoped_refptr<DevToolsAdbBridge::RemoteBrowser> current_browser() const {
    return browsers_.back();
  }

  void NextBrowser();
  void NextDevice();

  void Respond();

  void CreateBrowsers(const std::string& packages_response,
                      const std::string& processes_response,
                      const std::string& sockets_response);

  void ParseDumpsysResponse(const std::string& response);
  void ParseScreenSize(const std::string& str);

  scoped_refptr<DevToolsAdbBridge> adb_bridge_;
  AndroidDeviceManager* device_manager_;
  base::MessageLoop* device_message_loop_;
  Callback callback_;
  std::vector<std::string> serials_;
  DevToolsAdbBridge::RemoteBrowsers browsers_;
  scoped_ptr<DevToolsAdbBridge::RemoteDevices> remote_devices_;
};

AdbPagesCommand::AdbPagesCommand(
    scoped_refptr<DevToolsAdbBridge> adb_bridge,
    AndroidDeviceManager* device_manager,
    base::MessageLoop* device_message_loop,
    const DevToolsAdbBridge::DeviceProviders& device_providers,
    const Callback& callback)
    : adb_bridge_(adb_bridge),
      device_manager_(device_manager),
      device_message_loop_(device_message_loop),
      callback_(callback) {
  remote_devices_.reset(new DevToolsAdbBridge::RemoteDevices());

  device_message_loop_->PostTask(
      FROM_HERE, base::Bind(
          &AndroidDeviceManager::QueryDevices,
          device_manager_,
          device_providers,
          base::Bind(&AdbPagesCommand::ReceivedSerials, this)));
}

AdbPagesCommand::~AdbPagesCommand() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void AdbPagesCommand::ReceivedSerials(const std::vector<std::string>& serials) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  serials_ = serials;
  ProcessSerials();
}

void AdbPagesCommand::ProcessSerials() {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (serials_.size() == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbPagesCommand::Respond, this));
    return;
  }

  if (device_manager_->IsConnected(current_serial())) {
    device_manager_->RunCommand(current_serial(), kDeviceModelCommand,
                       base::Bind(&AdbPagesCommand::ReceivedModel, this));
  } else {
    remote_devices_->push_back(new DevToolsAdbBridge::RemoteDevice(
        adb_bridge_, current_serial(), kUnknownModel, false));
    NextDevice();
  }
}

void AdbPagesCommand::ReceivedModel(int result, const std::string& response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result < 0) {
    NextDevice();
    return;
  }
  remote_devices_->push_back(new DevToolsAdbBridge::RemoteDevice(
      adb_bridge_, current_serial(), response, true));
  device_manager_->RunCommand(current_serial(), kDumpsysCommand,
      base::Bind(&AdbPagesCommand::ReceivedDumpsys, this));
}

void AdbPagesCommand::ReceivedDumpsys(int result,
                                      const std::string& response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result >= 0)
    ParseDumpsysResponse(response);

  device_manager_->RunCommand(
      current_serial(),
      kInstalledChromePackagesCommand,
      base::Bind(&AdbPagesCommand::ReceivedPackages, this));
}

void AdbPagesCommand::ReceivedPackages(int result,
                                       const std::string& packages_response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result < 0) {
    NextDevice();
    return;
  }
  device_manager_->RunCommand(
      current_serial(),
      kListProcessesCommand,
      base::Bind(&AdbPagesCommand::ReceivedProcesses, this, packages_response));
}

void AdbPagesCommand::ReceivedProcesses(
    const std::string& packages_response,
    int result,
    const std::string& processes_response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result < 0) {
    NextDevice();
    return;
  }
  device_manager_->RunCommand(
      current_serial(),
      kOpenedUnixSocketsCommand,
      base::Bind(&AdbPagesCommand::ReceivedSockets,
                 this,
                 packages_response,
                 processes_response));
}

void AdbPagesCommand::ReceivedSockets(
    const std::string& packages_response,
    const std::string& processes_response,
    int result,
    const std::string& sockets_response) {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (result >= 0)
    CreateBrowsers(packages_response, processes_response, sockets_response);
  ProcessSockets();
}

void AdbPagesCommand::ProcessSockets() {
  DCHECK_EQ(device_message_loop_, base::MessageLoop::current());
  if (browsers_.size() == 0) {
    NextDevice();
    return;
  }

  device_manager_->HttpQuery(
      current_serial(),
      current_browser()->socket(),
      kVersionRequest,
      base::Bind(&AdbPagesCommand::ReceivedVersion, this));
}

void AdbPagesCommand::ReceivedVersion(int result,
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
      const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
      if (descriptor)
        current_browser()->set_display_name(descriptor->display_name);
    }
  }

  device_manager_->HttpQuery(
      current_serial(),
      current_browser()->socket(),
      kPageListRequest,
      base::Bind(&AdbPagesCommand::ReceivedPages, this));
}

void AdbPagesCommand::ReceivedPages(int result,
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

void AdbPagesCommand::NextBrowser() {
  browsers_.pop_back();
  ProcessSockets();
}

void AdbPagesCommand::NextDevice() {
  serials_.pop_back();
  ProcessSerials();
}

void AdbPagesCommand::Respond() {
  callback_.Run(remote_devices_.release());
}

void AdbPagesCommand::CreateBrowsers(
    const std::string& packages_response,
    const std::string& processes_response,
    const std::string& sockets_response) {
  DescriptorMap package_to_descriptor =
      FindInstalledBrowserPackages(packages_response);

  StringMap pid_to_package;
  StringMap package_to_pid;
  MapProcessesToPackages(processes_response, pid_to_package, package_to_pid);

  const std::string channel_pattern =
      base::StringPrintf(kDevToolsChannelNameFormat, "");

  StringMap socket_to_pid = MapSocketsToProcesses(sockets_response,
                                                  channel_pattern);

  scoped_refptr<DevToolsAdbBridge::RemoteDevice> remote_device =
      remote_devices_->back();

  // Create RemoteBrowser instances.
  BrowserMap package_to_running_browser;
  BrowserMap socket_to_unnamed_browser;
  for (StringMap::iterator it = socket_to_pid.begin();
      it != socket_to_pid.end(); ++it) {
    std::string socket = it->first;
    std::string pid = it->second;

    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser =
        new DevToolsAdbBridge::RemoteBrowser(
            adb_bridge_, current_serial(), socket);

    StringMap::iterator pit = pid_to_package.find(pid);
    if (pit != pid_to_package.end()) {
      std::string package = pit->second;
      package_to_running_browser[package] = browser;
      const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
      if (descriptor) {
        browser->set_display_name(descriptor->display_name);
      } else if (socket.find(kWebViewSocketPrefix) == 0) {
        browser->set_display_name(
            base::StringPrintf(kWebViewNameTemplate, package.c_str()));
      } else {
        browser->set_display_name(package);
      }
    } else {
      // Set fallback display name.
      std::string name = socket.substr(0, socket.find(channel_pattern));
      name[0] = base::ToUpperASCII(name[0]);
      browser->set_display_name(name);

      socket_to_unnamed_browser[socket] = browser;
    }
    remote_device->AddBrowser(browser);
  }

  browsers_ = remote_device->browsers();

  // Find installed packages not mapped to browsers.
  typedef std::multimap<std::string, const BrowserDescriptor*>
      DescriptorMultimap;
  DescriptorMultimap socket_to_descriptor;
  for (DescriptorMap::iterator it = package_to_descriptor.begin();
      it != package_to_descriptor.end(); ++it) {
    std::string package = it->first;
    const BrowserDescriptor* descriptor = it->second;

    if (package_to_running_browser.find(package) !=
        package_to_running_browser.end())
      continue;  // This package is already mapped to a browser.

    if (package_to_pid.find(package) != package_to_pid.end()) {
      // This package is running but not mapped to a browser.
      socket_to_descriptor.insert(
          DescriptorMultimap::value_type(descriptor->socket, descriptor));
      continue;
    }
  }

  // Try naming remaining unnamed browsers.
  for (DescriptorMultimap::iterator it = socket_to_descriptor.begin();
      it != socket_to_descriptor.end(); ++it) {
    std::string socket = it->first;
    const BrowserDescriptor* descriptor = it->second;

    if (socket_to_descriptor.count(socket) != 1)
      continue;  // No definitive match.

    BrowserMap::iterator bit = socket_to_unnamed_browser.find(socket);
    if (bit != socket_to_unnamed_browser.end())
      bit->second->set_display_name(descriptor->display_name);
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

class AdbProtocolCommand : public DevToolsAdbBridge::AdbWebSocket::Delegate {
 public:
  AdbProtocolCommand(
      scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser,
      const std::string& debug_url,
      const std::string& command);

 private:
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;

  const std::string command_;
  scoped_refptr<DevToolsAdbBridge::AdbWebSocket> web_socket_;

  DISALLOW_COPY_AND_ASSIGN(AdbProtocolCommand);
};

AdbProtocolCommand::AdbProtocolCommand(
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser,
    const std::string& debug_url,
    const std::string& command)
    : command_(command) {
  web_socket_ = browser->CreateWebSocket(debug_url, this);
}

void AdbProtocolCommand::OnSocketOpened() {
  web_socket_->SendFrame(command_);
  web_socket_->Disconnect();
}

void AdbProtocolCommand::OnFrameRead(const std::string& message) {}

void AdbProtocolCommand::OnSocketClosed(bool closed_by_device) {
  delete this;
}

}  // namespace

const char kDevToolsChannelNameFormat[] = "%s_devtools_remote";

class AgentHostDelegate;

typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;

base::LazyInstance<AgentHostDelegates>::Leaky g_host_delegates =
    LAZY_INSTANCE_INITIALIZER;

DevToolsAdbBridge::Wrapper::Wrapper(content::BrowserContext* context) {
  bridge_ = new DevToolsAdbBridge(Profile::FromBrowserContext(context));
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

KeyedService* DevToolsAdbBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevToolsAdbBridge::Wrapper(context);
}


// AgentHostDelegate ----------------------------------------------------------

class AgentHostDelegate : public content::DevToolsExternalAgentProxyDelegate,
                          public DevToolsAdbBridge::AdbWebSocket::Delegate {
 public:
  static void Create(const std::string& id,
                     scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser,
                     const std::string& debug_url,
                     const std::string& frontend_url,
                     Profile* profile) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    AgentHostDelegates::iterator it =
        g_host_delegates.Get().find(id);
    if (it != g_host_delegates.Get().end()) {
      it->second->OpenFrontend();
    } else if (!frontend_url.empty()) {
      new AgentHostDelegate(
          id, browser, debug_url, frontend_url, profile);
    }
  }

 private:
  AgentHostDelegate(
      const std::string& id,
      scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser,
      const std::string& debug_url,
      const std::string& frontend_url,
      Profile* profile)
      : id_(id),
        frontend_url_(frontend_url),
        profile_(profile) {
    web_socket_ = browser->CreateWebSocket(debug_url, this);
    g_host_delegates.Get()[id] = this;

    if (browser->socket().find(kWebViewSocketPrefix) == 0) {
      content::RecordAction(
          base::UserMetricsAction("DevTools_InspectAndroidWebView"));
    } else {
      content::RecordAction(
          base::UserMetricsAction("DevTools_InspectAndroidPage"));
    }
  }

  void OpenFrontend() {
    if (!proxy_)
      return;
    DevToolsWindow::OpenExternalFrontend(
        profile_, frontend_url_, proxy_->GetAgentHost().get());
  }

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

  const std::string id_;
  const std::string frontend_url_;
  Profile* profile_;

  scoped_ptr<content::DevToolsExternalAgentProxy> proxy_;
  scoped_refptr<DevToolsAdbBridge::AdbWebSocket> web_socket_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

//// RemotePageTarget ----------------------------------------------

class RemotePageTarget : public DevToolsTargetImpl {
 public:
  RemotePageTarget(scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser,
                   const base::DictionaryValue& value);
  virtual ~RemotePageTarget();

  // content::DevToolsTarget overrides:
  virtual bool IsAttached() const OVERRIDE;
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

  // DevToolsTargetImpl overrides:
  virtual void Inspect(Profile* profile) const OVERRIDE;
  virtual void Reload() const OVERRIDE;

  void Navigate(const std::string& url) const;

 private:
  scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser_;
  std::string debug_url_;
  std::string frontend_url_;
  std::string remote_id_;
  DISALLOW_COPY_AND_ASSIGN(RemotePageTarget);
};

RemotePageTarget::RemotePageTarget(
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser,
    const base::DictionaryValue& value)
    : browser_(browser) {
  type_ = "adb_page";
  value.GetString("id", &remote_id_);
  std::string url;
  value.GetString("url", &url);
  url_ = GURL(url);
  value.GetString("title", &title_);
  title_ = base::UTF16ToUTF8(net::UnescapeForHTML(base::UTF8ToUTF16(title_)));
  value.GetString("description", &description_);
  std::string favicon_url;
  value.GetString("faviconUrl", &favicon_url);
  favicon_url_ = GURL(favicon_url);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
  value.GetString("devtoolsFrontendUrl", &frontend_url_);

  if (remote_id_.empty() && !debug_url_.empty())  {
    // Target id is not available until Chrome 26. Use page id at the end of
    // debug_url_ instead. For attached targets the id will remain empty.
    std::vector<std::string> parts;
    Tokenize(debug_url_, "/", &parts);
    remote_id_ = parts[parts.size()-1];
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

  id_ = base::StringPrintf("%s:%s:%s",
      browser_->serial().c_str(),
      browser_->socket().c_str(),
      remote_id_.c_str());
}

RemotePageTarget::~RemotePageTarget() {
}

bool RemotePageTarget::IsAttached() const {
  return debug_url_.empty();
}

static void NoOp(int, const std::string&) {}

static void CallClosure(base::Closure closure, int, const std::string&) {
  closure.Run();
}

void RemotePageTarget::Inspect(Profile* profile) const {
  std::string request = base::StringPrintf(kActivatePageRequest,
                                           remote_id_.c_str());
  base::Closure inspect_callback = base::Bind(&AgentHostDelegate::Create,
      id_, browser_, debug_url_, frontend_url_, profile);
  browser_->SendJsonRequest(
      request, base::Bind(&CallClosure, inspect_callback));
}

bool RemotePageTarget::Activate() const {
  std::string request = base::StringPrintf(kActivatePageRequest,
                                           remote_id_.c_str());
  browser_->SendJsonRequest(request, base::Bind(&NoOp));
  return true;
}

bool RemotePageTarget::Close() const {
  if (IsAttached())
    return false;
  std::string request = base::StringPrintf(kClosePageRequest,
                                           remote_id_.c_str());
  browser_->SendJsonRequest(request, base::Bind(&NoOp));
  return true;
}

void RemotePageTarget::Reload() const {
  browser_->SendProtocolCommand(debug_url_, kPageReloadCommand, NULL);
}

void RemotePageTarget::Navigate(const std::string& url) const {
  base::DictionaryValue params;
  params.SetString(kUrlParam, url);
  browser_->SendProtocolCommand(debug_url_, kPageNavigateCommand, &params);
}

// DevToolsAdbBridge::RemoteBrowser -------------------------------------------

DevToolsAdbBridge::RemoteBrowser::RemoteBrowser(
    scoped_refptr<DevToolsAdbBridge> adb_bridge,
    const std::string& serial,
    const std::string& socket)
    : adb_bridge_(adb_bridge),
      serial_(serial),
      socket_(socket),
      page_descriptors_(new base::ListValue()) {
}

bool DevToolsAdbBridge::RemoteBrowser::IsChrome() const {
  return socket_.find(kChromeDefaultSocket) == 0;
}

DevToolsAdbBridge::RemoteBrowser::ParsedVersion
DevToolsAdbBridge::RemoteBrowser::GetParsedVersion() const {
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

std::vector<DevToolsTargetImpl*>
DevToolsAdbBridge::RemoteBrowser::CreatePageTargets() {
  std::vector<DevToolsTargetImpl*> result;
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

void DevToolsAdbBridge::RemoteBrowser::SetPageDescriptors(
    const base::ListValue& list) {
  page_descriptors_.reset(list.DeepCopy());
}

static void RespondOnUIThread(
    const DevToolsAdbBridge::RemoteBrowser::JsonRequestCallback& callback,
    int result,
    const std::string& response) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result, response));
}

void DevToolsAdbBridge::RemoteBrowser::SendJsonRequest(
    const std::string& request, const JsonRequestCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  adb_bridge_->device_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidDeviceManager::HttpQuery,
                 adb_bridge_->device_manager(), serial_, socket_, request,
                 base::Bind(&RespondOnUIThread, callback)));
}

void DevToolsAdbBridge::RemoteBrowser::SendProtocolCommand(
    const std::string& debug_url,
    const std::string& method,
    base::DictionaryValue* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (debug_url.empty())
    return;
  DevToolsProtocol::Command command(1, method, params);
  new AdbProtocolCommand(this, debug_url, command.Serialize());
}

void DevToolsAdbBridge::RemoteBrowser::Open(const std::string& input_url) {
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
    SendJsonRequest(request, base::Bind(&NoOp));
  } else {
    SendJsonRequest(kNewPageRequest,
        base::Bind(&RemoteBrowser::PageCreatedOnHandlerThread, this, url));
  }
}

void DevToolsAdbBridge::RemoteBrowser::PageCreatedOnHandlerThread(
    const std::string& url, int result, const std::string& response) {
  if (result < 0)
    return;
  // Navigating too soon after the page creation breaks navigation history
  // (crbug.com/311014). This can be avoided by adding a moderate delay.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemoteBrowser::PageCreatedOnUIThread, this, response, url),
      base::TimeDelta::FromMilliseconds(kNewPageNavigateDelayMs));
}

void DevToolsAdbBridge::RemoteBrowser::PageCreatedOnUIThread(
    const std::string& response, const std::string& url) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    RemotePageTarget new_page(this, *dict);
    new_page.Navigate(url);
  }
}

DevToolsAdbBridge::RemoteBrowser::~RemoteBrowser() {
}


// DevToolsAdbBridge::RemoteDevice --------------------------------------------

DevToolsAdbBridge::RemoteDevice::RemoteDevice(
    scoped_refptr<DevToolsAdbBridge> adb_bridge,
    const std::string& serial,
    const std::string& model,
    bool connected)
    : adb_bridge_(adb_bridge),
      serial_(serial),
      model_(model),
      connected_(connected) {
}

void DevToolsAdbBridge::RemoteDevice::AddBrowser(
    scoped_refptr<RemoteBrowser> browser) {
  browsers_.push_back(browser);
}

void DevToolsAdbBridge::RemoteDevice::OpenSocket(
    const std::string& socket_name,
    const AndroidDeviceManager::SocketCallback& callback) {
  adb_bridge_->device_message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDeviceManager::OpenSocket,
                 adb_bridge_->device_manager(),
                 serial_,
                 socket_name,
                 callback));
}

DevToolsAdbBridge::RemoteDevice::~RemoteDevice() {
}


// DevToolsAdbBridge ----------------------------------------------------------

DevToolsAdbBridge::DevToolsAdbBridge(Profile* profile)
    : profile_(profile),
      adb_thread_(RefCountedAdbThread::GetInstance()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&DevToolsAdbBridge::CreateDeviceProviders,
                 base::Unretained(this)));
  CreateDeviceProviders();
  base::PostTaskAndReplyWithResult(
      device_message_loop()->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&AndroidDeviceManager::Create),
      base::Bind(&DevToolsAdbBridge::CreatedDeviceManager, this));
}

void DevToolsAdbBridge::AddListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  listeners_.push_back(listener);
  if (listeners_.size() == 1 && device_manager_)
    RequestRemoteDevices();
}

void DevToolsAdbBridge::RemoveListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Listeners::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
  if (listeners_.empty() && device_manager_) {
    device_message_loop()->PostTask(FROM_HERE,
        base::Bind(&AndroidDeviceManager::Stop, device_manager_));
  }
}

// static
bool DevToolsAdbBridge::HasDevToolsWindow(const std::string& agent_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_host_delegates.Get().find(agent_id) != g_host_delegates.Get().end();
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(listeners_.empty());
  if (device_manager_)
    device_message_loop()->PostTask(FROM_HERE,
        base::Bind(&AndroidDeviceManager::Stop, device_manager_));
}

void DevToolsAdbBridge::RequestRemoteDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device_manager_);

  if (listeners_.empty())
    return;

  new AdbPagesCommand(
      this,
      device_manager(),
      device_message_loop(),
      device_providers_,
      base::Bind(&DevToolsAdbBridge::ReceivedRemoteDevices, this));
}

void DevToolsAdbBridge::CreatedDeviceManager(
    scoped_refptr<AndroidDeviceManager> device_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_manager_ = device_manager;
  if (!listeners_.empty())
    RequestRemoteDevices();
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

void DevToolsAdbBridge::CreateDeviceProviders() {
  DevToolsAdbBridge::DeviceProviders device_providers;
#if defined(DEBUG_DEVTOOLS)
  device_providers.push_back(AndroidDeviceProvider::GetSelfAsDeviceProvider());
#endif
  device_providers.push_back(AndroidDeviceProvider::GetAdbDeviceProvider());

  PrefService* service = profile_->GetPrefs();
  const PrefService::Preference* pref =
      service->FindPreference(prefs::kDevToolsDiscoverUsbDevicesEnabled);
  const base::Value* pref_value = pref->GetValue();

  bool enabled;
  if (pref_value->GetAsBoolean(&enabled) && enabled) {
    device_providers.push_back(
        AndroidDeviceProvider::GetUsbDeviceProvider(profile_));
  }

  set_device_providers(device_providers);
}
