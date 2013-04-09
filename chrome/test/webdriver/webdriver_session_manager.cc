// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_session_manager.h"

#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

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
  base::AutoLock lock(map_lock_);
  it = map_.find(id);
  if (it == map_.end())
    return false;
  map_.erase(it);
  return true;
}

Session* SessionManager::GetSession(const std::string& id) const {
  std::map<std::string, Session*>::const_iterator it;
  base::AutoLock lock(map_lock_);
  it = map_.find(id);
  if (it == map_.end())
    return NULL;
  return it->second;
}

void SessionManager::set_port(const std::string& port) {
  port_ = port;
}

void SessionManager::set_url_base(const std::string& url_base) {
  url_base_ = url_base;
}

std::string SessionManager::url_base() const {
  return url_base_;
}

SessionManager::SessionManager() {}

SessionManager::~SessionManager() {}

// static
SessionManager* SessionManager::GetInstance() {
  return Singleton<SessionManager>::get();
}

}  // namespace webdriver
