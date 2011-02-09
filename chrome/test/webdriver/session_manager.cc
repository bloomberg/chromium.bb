// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"

#if defined(OS_POSIX)
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(OS_WIN)
#include <Shellapi.h>
#include <Winsock2.h>
#endif

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
#endif  // Cann't use else since a warning will be generated.
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
    return false;
  }
  if (gethostname(host, sizeof host) != 0) {
    LOG(ERROR) << "Could not find the hostname of this machine";
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
  count_++;  // For every connection made increment the global count.
  for (int i = 0; i < 32; ++i) {
#ifdef OS_POSIX
    id += text[random() % (sizeof text - 1)];
#else
    id += text[rand() % (sizeof text - 1)];
#endif
  }
  session_generation_.Release();
  return id;
}

Session* SessionManager::Create() {
  std::string id = GenerateSessionID();
  {
    base::AutoLock lock(map_lock_);
    if (map_.find(id) != map_.end()) {
      LOG(ERROR) << "Failed to generate a unique session ID";
      return NULL;
    }
  }

  Session* session = new Session(id);
  base::AutoLock lock(map_lock_);
  map_[id] = session;
  return session;
}

bool SessionManager::Has(const std::string& id) const {
  base::AutoLock lock(map_lock_);
  return map_.find(id) != map_.end();
}

bool SessionManager::Delete(const std::string& id) {
  std::map<std::string, Session*>::iterator it;

  Session* session;
  {
    base::AutoLock lock(map_lock_);
    it = map_.find(id);
    if (it == map_.end()) {
      VLOG(1) << "No such session with ID " << id;
      return false;
    }
    session = it->second;
    map_.erase(it);
  }

  VLOG(1) << "Deleting session with ID " << id;
  delete session;
  return true;
}

Session* SessionManager::GetSession(const std::string& id) const {
  std::map<std::string, Session*>::const_iterator it;
  base::AutoLock lock(map_lock_);
  it = map_.find(id);
  if (it == map_.end()) {
    VLOG(1) << "No such session with ID " << id;
    return NULL;
  }
  return it->second;
}

SessionManager::SessionManager() : addr_(""), port_(""), count_(0) {}

SessionManager::~SessionManager() {}

// static
SessionManager* SessionManager::GetInstance() {
  return Singleton<SessionManager>::get();
}

}  // namespace webdriver
