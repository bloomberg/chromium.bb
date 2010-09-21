// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session_manager.h"

#ifdef OS_POSIX
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <net/if.h>
  #include <sys/ioctl.h>
  #include <sys/socket.h>
  #include <sys/types.h>
#elif OS_WIN
  #include <Shellapi.h>
  #include <Winsock2.h>
#endif

#include "base/command_line.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"

namespace webdriver {

std::string SessionManager::GetIPAddress() {
  return std::string(addr_) + std::string(":") + port_;
}

std::string SessionManager::IPLookup(const std::string& nic) {
#ifdef OS_POSIX
    int socket_conn;
    struct ifreq ifr;
    struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", nic.c_str());
    sin->sin_family = AF_INET;

    if (0 > (socket_conn = socket(AF_INET, SOCK_STREAM, 0))) {
      return std::string("");
    }

    if (0 == ioctl(socket_conn, SIOCGIFADDR, &ifr)) {
      return std::string(inet_ntoa(sin->sin_addr));
    }
#endif  // cann't use else since a warning will be generated
  return std::string("");
}

bool SessionManager::SetIPAddress(const std::string& p) {
  port_ = p;
  std::string prefix("192.");
#ifdef OS_POSIX
  char buff[32];

  for (int i = 0; i < 10; ++i) {
#ifdef OS_MACOSX
    snprintf(buff, sizeof(buff), "%s%d", "en", i);
#elif OS_LINUX
    snprintf(buff, sizeof(buff), "%s%d", "eth", i);
#endif
    addr_ = IPLookup(std::string(buff));
    if (addr_.length() > 0) {
      if ((addr_.compare("127.0.0.1") != 0) &&
          (addr_.compare("127.0.1.1") != 0) &&
          (addr_.compare(0, prefix.size(), prefix) != 0)) {
         return true;
      }
    }
  }
  return false;
#elif OS_WIN
  hostent *h;
  char host[1024];
  WORD wVersionRequested;
  WSADATA wsaData;

  memset(host, 0, sizeof host);
  wVersionRequested = MAKEWORD(2, 0);
  if (WSAStartup(wVersionRequested, &wsaData) != 0) {
    LOG(ERROR) << "Could not initialize the Windows Sockets library";
    LOG(ERROR)  << std::endl;
    return false;
  }
  if (gethostname(host, sizeof host) != 0) {
    LOG(ERROR) << "Could not find the hostname of this machine" << std::endl;
    WSACleanup();
    return false;
  }
  h = gethostbyname(host);
  for (int i = 0; ((h->h_addr_list[i]) && (i < h->h_length)); ++i) {
    addr_ = std::string(inet_ntoa(*(struct in_addr *)h->h_addr_list[i]));
    if ((addr_.compare("127.0.0.1") != 0) &&
        (addr_.compare("127.0.1.1") != 0) &&
        (addr_.compare(0, prefix.size(), prefix) != 0)) {
      WSACleanup();
      return true;
    }
  }

  WSACleanup();
  return false;
#endif
}

std::string SessionManager::GenerateSessionID() {
  static const char text[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQ"\
                             "RSTUVWXYZ0123456789";
  session_generation_.Acquire();
  std::string id = "";
  count_++;  // for every connection made increment the global count
  for (int i = 0; i < 32; ++i) {
#ifdef OS_POSIX
    id += text[random() % (sizeof text - 1)];
#else
    id += text[rand() % (sizeof text - 1)];
#endif
    id += count_;  // append the global count to generate a unique id
  }
  session_generation_.Release();
  return id;
}

bool SessionManager::Create(std::string* id) {
  FilePath browser_directory;   // Path to the browser executable
  MessageLoop loop;

  *id = GenerateSessionID();
  if (map_.find(*id) != map_.end()) {
    LOG(ERROR) << "Failed to generate a unique session ID";
    return false;
  }

  // Init the commandline singleton from the env
  CommandLine::Init(0, NULL);

  // start chrome, if it doesn't startup in 8 seconds quit
  scoped_ptr<Session> session(new Session(*id, new AutomationProxy(8000,
                                                                   false)));
  if (session->proxy() == NULL) {
    LOG(WARNING) << "Could not allocate automation proxy" << std::endl;
    return false;
  }

#if defined(OS_POSIX)
  char* user_data_dir = getenv("CHROME_UI_TESTS_USER_DATA_DIR");
  if (user_data_dir) {
    browser_directory = browser_directory.Append(user_data_dir);
  }
#endif

  FilePath command =
    browser_directory.Append(FilePath::FromWStringHack(
                             chrome::kBrowserProcessExecutablePath));
  CommandLine command_line(command);

  // setup the automation port to communicate on
  command_line.AppendSwitchASCII(switches::kTestingChannelID,
                                 session->proxy()->channel_id());

  // No first-run dialogs, please.
  command_line.AppendSwitch(switches::kNoFirstRun);

  // No default browser check, it would create an info-bar (if we are
  // not the default browser) that could conflicts with some tests
  // expectations.
  command_line.AppendSwitch(switches::kNoDefaultBrowserCheck);

  // We need cookies on file:// for things like the page cycler.
  command_line.AppendSwitch(switches::kEnableFileCookies);
  command_line.AppendSwitch(switches::kDomAutomationController);
  command_line.AppendSwitch(switches::kFullMemoryCrashReport);

  // create a temp directory for the new profile
  if (!session->CreateTemporaryProfileDirectory()) {
    LOG(ERROR) << "Could not make a temp profile directory, "
               << session->tmp_profile_dir() << std::endl;
    LOG(ERROR) << "Need to quit, the issue must be fixed" << std::endl;
    exit(-1);
  }

  command_line.AppendSwitchASCII(switches::kUserDataDir,
                                 session->tmp_profile_dir());

  base::ProcessHandle process_handle;
#if defined(OS_WIN)
  base::LaunchApp(command_line, false, true, &process_handle);
#elif defined(OS_POSIX)
  // Sometimes one needs to run the browser under a special environment
  // (e.g. valgrind) without also running the test harness (e.g. python)
  // under the special environment.  Provide a way to wrap the browser
  // commandline with a special prefix to invoke the special environment.
  const char* browser_wrapper = getenv("BROWSER_WRAPPER");

  if (browser_wrapper) {
    command_line.PrependWrapper(browser_wrapper);
    LOG(INFO) << "BROWSER_WRAPPER was set, prefixing command_line with "
              << browser_wrapper;
  }

  base::LaunchApp(command_line.argv(), session->proxy()->fds_to_map(), false,
      &process_handle);
#endif

  if (!session->Init(process_handle)) {
    LOG(WARNING) << "Failed to initialize session";
    return false;
  }

  LOG(INFO) << "New session was created: " << id << std::endl;
  map_[*id] = session.release();
  return true;
}

bool SessionManager::Has(const std::string& id) const {
  return map_.find(id) != map_.end();
}

bool SessionManager::Delete(const std::string& id) {
  std::map<std::string, Session*>::iterator it;

  LOG(INFO) << "Deleting session with ID " << id;
  it = map_.find(id);
  if (it == map_.end()) {
    LOG(INFO) << "No such session with ID " << id;
    return false;
  }

  it->second->Terminate();
  map_.erase(it);
  return true;
}

Session* SessionManager::GetSession(const std::string& id) const {
  std::map<std::string, Session*>::const_iterator it;
  it = map_.find(id);
  if (it == map_.end()) {
    LOG(INFO) << "No such session with ID " << id;
    return NULL;
  }
  return it->second;
}
}  // namespace webdriver
