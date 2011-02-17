// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session_manager.h"

#include "base/logging.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#endif

namespace webdriver {

std::string SessionManager::GetAddress() {
  std::string hostname = net::GetHostName();
#if defined(OS_WIN)
  if (hostname.length()) {
    // Get the fully qualified name.
    struct hostent* host_entry = gethostbyname(hostname.c_str());
    if (host_entry)
      hostname = host_entry->h_name;
  }
#endif
  if (hostname.empty()) {
    hostname = "localhost";
  }
  return hostname + ":" + port_;
}

void SessionManager::Add(Session* session) {
  base::AutoLock lock(map_lock_);
  map_[session->id()] = session;
}

bool SessionManager::Has(const std::string& id) const {
  base::AutoLock lock(map_lock_);
  return map_.find(id) != map_.end();
}

bool SessionManager::Remove(const std::string& id) {
  std::map<std::string, Session*>::iterator it;
  Session* session;
  base::AutoLock lock(map_lock_);
  it = map_.find(id);
  if (it == map_.end()) {
    VLOG(1) << "No such session with ID " << id;
    return false;
  }
  session = it->second;
  map_.erase(it);
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

SessionManager::SessionManager() : port_("") {}

SessionManager::~SessionManager() {}

// static
SessionManager* SessionManager::GetInstance() {
  return Singleton<SessionManager>::get();
}

}  // namespace webdriver
