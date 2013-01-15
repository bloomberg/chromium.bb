// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include "chrome/test/chromedriver/chrome.h"

Session::Session(const std::string& id)
    : id(id), implicit_wait(0), page_load_timeout(0), script_timeout(0) {}

Session::Session(const std::string& id, scoped_ptr<Chrome> chrome)
    : id(id), chrome(chrome.Pass()), implicit_wait(0),
      page_load_timeout(0), script_timeout(0) {}

Session::~Session() {}

SessionAccessorImpl::SessionAccessorImpl(scoped_ptr<Session> session)
    : session_(session.Pass()) {}

Session* SessionAccessorImpl::Access(scoped_ptr<base::AutoLock>* lock) {
  lock->reset(new base::AutoLock(session_lock_));
  return session_.get();
}

SessionAccessorImpl::~SessionAccessorImpl() {}
