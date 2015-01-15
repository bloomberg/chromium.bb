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
const char kWindowPolicyCommand[] = "shell:dumpsys window policy";
const char kListUsersCommand[] = "shell:dumpsys user";
const char kScreenSizePrefix[] = "mStable=";
const char kUserInfoPrefix[] = "UserInfo{";

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
  for (int i = 0; i < count; i++) {
    if (kBrowserDescriptors[i].package == package)
      return &kBrowserDescriptors[i];
  }
  return nullptr;
}

using StringMap = std::map<std::string, std::string>;

void MapProcessesToPackages(const std::string& response,
                            StringMap* pid_to_package,
                            StringMap* pid_to_user) {
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
    (*pid_to_package)[pid] = fields[8];
    (*pid_to_user)[pid] = fields[0];
  }
}

StringMap MapSocketsToProcesses(const std::string& response) {
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
    size_t socket_name_pos = path_field.find(kDevToolsSocketSuffix);
    if (socket_name_pos == std::string::npos)
      continue;

    std::string socket = path_field.substr(1);

    std::string pid;
    size_t socket_name_end = socket_name_pos + strlen(kDevToolsSocketSuffix);
    if (socket_name_end < path_field.size() &&
        path_field[socket_name_end] == '_') {
      pid = path_field.substr(socket_name_end + 1);
    }
    socket_to_pid[socket] = pid;
  }
  return socket_to_pid;
}

gfx::Size ParseScreenSize(const std::string& str) {
  std::vector<std::string> pairs;
  Tokenize(str, "-", &pairs);
  if (pairs.size() != 2)
    return gfx::Size();

  int width;
  int height;
  std::vector<std::string> numbers;
  Tokenize(pairs[1].substr(1, pairs[1].size() - 2), ",", &numbers);
  if (numbers.size() != 2 ||
      !base::StringToInt(numbers[0], &width) ||
      !base::StringToInt(numbers[1], &height))
    return gfx::Size();

  return gfx::Size(width, height);
}

gfx::Size ParseWindowPolicyResponse(const std::string& response) {
  std::vector<std::string> lines;
  Tokenize(response, "\r", &lines);
  for (const std::string& line : lines) {
    size_t pos = line.find(kScreenSizePrefix);
    if (pos != std::string::npos) {
      return ParseScreenSize(
          line.substr(pos + strlen(kScreenSizePrefix)));
    }
  }
  return gfx::Size();
}

StringMap MapIdsToUsers(const std::string& response) {
  // Parse 'dumpsys user' output which looks like this:
  // Users:
  //   UserInfo{0:Test User:13} serialNo=0
  //     Created: <unknown>
  //     Last logged in: +17m18s871ms ago
  //   UserInfo{10:User with : (colon):10} serialNo=10
  //     Created: +3d4h35m1s139ms ago
  //     Last logged in: +17m26s287ms ago

  StringMap id_to_username;
  std::vector<std::string> lines;
  Tokenize(response, "\r", &lines);
  for (const std::string& line : lines) {
    size_t pos = line.find(kUserInfoPrefix);
    if (pos != std::string::npos) {
      std::string fields = line.substr(pos + strlen(kUserInfoPrefix));
      size_t first_pos = fields.find_first_of(":");
      size_t last_pos = fields.find_last_of(":");
      if (first_pos != std::string::npos && last_pos != std::string::npos) {
        std::string id = fields.substr(0, first_pos);
        std::string name = fields.substr(first_pos + 1,
                                         last_pos - first_pos - 1);
        id_to_username[id] = name;
      }
    }
  }
  return id_to_username;
}

std::string GetUserName(const std::string& unix_user,
                        const StringMap id_to_username) {
  // Parse username as returned by ps which looks like 'u0_a31'
  // where '0' is user id and '31' is app id.
  if (!unix_user.empty() && unix_user[0] == 'u') {
    size_t pos = unix_user.find('_');
    if (pos != std::string::npos) {
      StringMap::const_iterator it =
          id_to_username.find(unix_user.substr(1, pos - 1));
      if (it != id_to_username.end())
        return it->second;
    }
  }
  return std::string();
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
    : callback_(callback) {
  AddRef();
  command_callback.Run(
      kDeviceModelCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedModel, this));
  command_callback.Run(
      kWindowPolicyCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedWindowPolicy, this));
  command_callback.Run(
      kListProcessesCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedProcesses, this));
  command_callback.Run(
      kOpenedUnixSocketsCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedSockets, this));
  command_callback.Run(
      kListUsersCommand,
      base::Bind(&AdbDeviceInfoQuery::ReceivedUsers, this));
  Release();
}

AdbDeviceInfoQuery::~AdbDeviceInfoQuery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ParseBrowserInfo();
  callback_.Run(device_info_);
}

void AdbDeviceInfoQuery::ReceivedModel(int result,
                                       const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result >= 0) {
    TrimWhitespaceASCII(response, base::TRIM_ALL, &device_info_.model);
    device_info_.connected = true;
  }
}

void AdbDeviceInfoQuery::ReceivedWindowPolicy(int result,
                                              const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result >= 0)
    device_info_.screen_size = ParseWindowPolicyResponse(response);
}

void AdbDeviceInfoQuery::ReceivedProcesses(int result,
                                           const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result >= 0)
    processes_response_ = response;
}

void AdbDeviceInfoQuery::ReceivedSockets(int result,
                                         const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result >= 0)
    sockets_response_ = response;
}

void AdbDeviceInfoQuery::ReceivedUsers(int result,
                                       const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result >= 0)
    users_response_ = response;
}

void AdbDeviceInfoQuery::ParseBrowserInfo() {
  StringMap pid_to_package;
  StringMap pid_to_user;
  MapProcessesToPackages(processes_response_,
                         &pid_to_package,
                         &pid_to_user);
  StringMap socket_to_pid = MapSocketsToProcesses(sockets_response_);
  StringMap id_to_username = MapIdsToUsers(users_response_);
  std::set<std::string> used_pids;
  for (const auto& pair : socket_to_pid)
    used_pids.insert(pair.second);

  for (const auto& pair : pid_to_package) {
    std::string pid = pair.first;
    std::string package = pair.second;
    if (used_pids.find(pid) == used_pids.end()) {
      const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
      if (descriptor)
        socket_to_pid[descriptor->socket] = pid;
    }
  }

  for (const auto& pair : socket_to_pid) {
    std::string socket = pair.first;
    std::string pid = pair.second;
    std::string package;
    StringMap::iterator pit = pid_to_package.find(pid);
    if (pit != pid_to_package.end())
      package = pit->second;

    AndroidDeviceManager::BrowserInfo browser_info;
    browser_info.socket_name = socket;
    browser_info.type = GetBrowserType(socket);
    browser_info.display_name = GetDisplayName(socket, package);

    StringMap::iterator uit = pid_to_user.find(pid);
    if (uit != pid_to_user.end())
      browser_info.user = GetUserName(uit->second, id_to_username);

    device_info_.browser_info.push_back(browser_info);
  }
}
