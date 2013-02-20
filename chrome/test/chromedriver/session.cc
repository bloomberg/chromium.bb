// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include <list>

#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/web_view.h"

Session::Session(const std::string& id)
    : id(id), mouse_position(0, 0), implicit_wait(0), page_load_timeout(0),
      script_timeout(0) {}

Session::Session(const std::string& id, scoped_ptr<Chrome> chrome)
    : id(id), chrome(chrome.Pass()), mouse_position(0, 0), implicit_wait(0),
      page_load_timeout(0), script_timeout(0) {}

Session::~Session() {}

Status Session::GetTargetWindow(WebView** web_view) {
  if (!chrome)
    return Status(kNoSuchWindow, "no chrome started in this session");

  std::list<WebView*> web_views;
  Status status = chrome->GetWebViews(&web_views);
  if (status.IsError())
    return status;

  for (std::list<WebView*>::const_iterator it = web_views.begin();
       it != web_views.end(); ++it) {
    if ((*it)->GetId() == window) {
      *web_view = *it;
      return Status(kOk);
    }
  }
  return Status(kNoSuchWindow, "target window already closed");
}

SessionAccessorImpl::SessionAccessorImpl(scoped_ptr<Session> session)
    : session_(session.Pass()) {}

Session* SessionAccessorImpl::Access(scoped_ptr<base::AutoLock>* lock) {
  lock->reset(new base::AutoLock(session_lock_));
  return session_.get();
}

SessionAccessorImpl::~SessionAccessorImpl() {}
