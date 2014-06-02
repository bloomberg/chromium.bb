// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/adb_device_info_query.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {


const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kListProcessesCommand[] = "shell:ps";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kDumpsysScreenSizePrefix[] = "mStable=";

const char kDevToolsSocketSuffix[] = "_devtools_remote";

const char kChromeDefaultName[] = "Chrome";
const char kChromeDefaultSocket[] = "chrome_devtools_remote";

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

}  // namespace

// static
AndroidDeviceManager::BrowserInfo::Type
AdbDeviceInfoQuery::GetBrowserType(const std::string& socket) {
  if (socket.find(kChromeDefaultSocket) == 0)
    return AndroidDeviceManager::BrowserInfo::kTypeChrome;

  if (socket.find(kWebViewSocketPrefix) == 0)
    return AndroidDeviceManager::BrowserInfo::kTypeWebView;

  return AndroidDeviceManager::BrowserInfo::kTypeOther;
}

// static
std::string AdbDeviceInfoQuery::GetDisplayName(const std::string& socket,
                                               const std::string& package) {
  if (package.empty()) {
    // Derive a fallback display name from the socket name.
    std::string name = socket.substr(0, socket.find(kDevToolsSocketSuffix));
    name[0] = base::ToUpperASCII(name[0]);
    return name;
  }

  const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
  if (descriptor)
    return descriptor->display_name;

  if (GetBrowserType(socket) ==
      AndroidDeviceManager::BrowserInfo::kTypeWebView)
    return base::StringPrintf(kWebViewNameTemplate, package.c_str());

  return package;
}

// static
void AdbDeviceInfoQuery::Start(const RunCommandCallback& command_callback,
                               const DeviceInfoCallback& callback) {
  new AdbDeviceInfoQuery(command_callback, callback);
}

AdbDeviceInfoQuery::AdbDeviceInfoQuery(
    const RunCommandCallback& command_callback,
    const DeviceInfoCallback& callback)
    : command_callback_(command_callback),
      callback_(callback) {
  DCHECK(CalledOnValidThread());
  command_callback_.Run(
      kDeviceModelCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedModel, base::Unretained(this)));
}

AdbDeviceInfoQuery::~AdbDeviceInfoQuery() {
}

void AdbDeviceInfoQuery::ReceivedModel(int result,
                                       const std::string& response) {
  DCHECK(CalledOnValidThread());
  if (result < 0) {
    Respond();
    return;
  }
  device_info_.model = response;
  device_info_.connected = true;
  command_callback_.Run(
      kDumpsysCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedDumpsys, base::Unretained(this)));
}

void AdbDeviceInfoQuery::ReceivedDumpsys(int result,
                                         const std::string& response) {
  DCHECK(CalledOnValidThread());
  if (result >= 0)
    ParseDumpsysResponse(response);

  command_callback_.Run(
      kListProcessesCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedProcesses,
                 base::Unretained(this)));
}

void AdbDeviceInfoQuery::ParseDumpsysResponse(const std::string& response) {
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

void AdbDeviceInfoQuery::ParseScreenSize(const std::string& str) {
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

  device_info_.screen_size = gfx::Size(width, height);
}


void AdbDeviceInfoQuery::ReceivedProcesses(
    int result,
    const std::string& processes_response) {
  DCHECK(CalledOnValidThread());
  if (result < 0) {
    Respond();
    return;
  }
  command_callback_.Run(
      kOpenedUnixSocketsCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedSockets,
                 base::Unretained(this),
                 processes_response));
}

void AdbDeviceInfoQuery::ReceivedSockets(
    const std::string& processes_response,
    int result,
    const std::string& sockets_response) {
  DCHECK(CalledOnValidThread());
  if (result >= 0)
    ParseBrowserInfo(processes_response, sockets_response);
  Respond();
}

void AdbDeviceInfoQuery::ParseBrowserInfo(
    const std::string& processes_response,
    const std::string& sockets_response) {
  DCHECK(CalledOnValidThread());
  StringMap pid_to_package;
  StringMap package_to_pid;
  MapProcessesToPackages(processes_response, pid_to_package, package_to_pid);

  StringMap socket_to_pid = MapSocketsToProcesses(sockets_response,
                                                  kDevToolsSocketSuffix);

  std::set<std::string> packages_for_running_browsers;

  typedef std::map<std::string, int> BrowserMap;
  BrowserMap socket_to_unnamed_browser_index;

  for (StringMap::iterator it = socket_to_pid.begin();
      it != socket_to_pid.end(); ++it) {
    std::string socket = it->first;
    std::string pid = it->second;

    std::string package;
    StringMap::iterator pit = pid_to_package.find(pid);
    if (pit != pid_to_package.end()) {
      package = pit->second;
      packages_for_running_browsers.insert(package);
    } else {
      socket_to_unnamed_browser_index[socket] =
          device_info_.browser_info.size();
    }

    AndroidDeviceManager::BrowserInfo browser_info;
    browser_info.socket_name = socket;
    browser_info.type = GetBrowserType(socket);
    browser_info.display_name = GetDisplayName(socket, package);
    device_info_.browser_info.push_back(browser_info);
  }
}

void AdbDeviceInfoQuery::Respond() {
  DCHECK(CalledOnValidThread());
  callback_.Run(device_info_);
  delete this;
}
