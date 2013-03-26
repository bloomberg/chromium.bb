// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include <list>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

FrameInfo::FrameInfo(const std::string& parent_frame_id,
                     const std::string& frame_id,
                     const std::string& chromedriver_frame_id)
    : parent_frame_id(parent_frame_id),
      frame_id(frame_id),
      chromedriver_frame_id(chromedriver_frame_id) {}

Session::Session(const std::string& id)
    : id(id),
      thread(("SessionThread_" + id).c_str()),
      mouse_position(0, 0),
      implicit_wait(0),
      page_load_timeout(0),
      script_timeout(0) {
}

Session::Session(const std::string& id, scoped_ptr<Chrome> chrome)
    : id(id),
      thread(("SessionThread_" + id).c_str()),
      chrome(chrome.Pass()),
      mouse_position(0, 0),
      implicit_wait(0),
      page_load_timeout(0),
      script_timeout(0),
      capabilities(CreateCapabilities()) {
}

Session::~Session() {}

Status Session::GetTargetWindow(WebView** web_view) {
  if (!chrome)
    return Status(kNoSuchWindow, "no chrome started in this session");

  Status status = chrome->GetWebViewById(window, web_view);
  if (status.IsError())
    status = Status(kNoSuchWindow, "target window already closed", status);
  return status;
}

void Session::SwitchToTopFrame() {
  frames.clear();
}

void Session::SwitchToSubFrame(const std::string& frame_id,
                               const std::string& chromedriver_frame_id) {
  std::string parent_frame_id;
  if (!frames.empty())
    parent_frame_id = frames.back().frame_id;
  frames.push_back(FrameInfo(parent_frame_id, frame_id, chromedriver_frame_id));
}

std::string Session::GetCurrentFrameId() const {
  if (frames.empty())
    return "";
  return frames.back().frame_id;
}

scoped_ptr<base::DictionaryValue> Session::CreateCapabilities() {
  scoped_ptr<base::DictionaryValue> caps(new base::DictionaryValue());
  caps->SetString("browserName", "chrome");
  caps->SetString("version", chrome->GetVersion());
  caps->SetString("driverVersion", kChromeDriverVersion);
  caps->SetString("platform", chrome->GetOperatingSystemName());
  caps->SetBoolean("javascriptEnabled", true);
  caps->SetBoolean("takesScreenshot", true);
  caps->SetBoolean("handlesAlerts", true);
  caps->SetBoolean("databaseEnabled", true);
  caps->SetBoolean("locationContextEnabled", true);
  caps->SetBoolean("applicationCacheEnabled", false);
  caps->SetBoolean("browserConnectionEnabled", false);
  caps->SetBoolean("cssSelectorsEnabled", true);
  caps->SetBoolean("webStorageEnabled", true);
  caps->SetBoolean("rotatable", false);
  caps->SetBoolean("acceptSslCerts", true);
  caps->SetBoolean("nativeEvents", true);
  return caps.Pass();
}

SessionAccessorImpl::SessionAccessorImpl(scoped_ptr<Session> session)
    : session_(session.Pass()) {}

Session* SessionAccessorImpl::Access(scoped_ptr<base::AutoLock>* lock) {
  lock->reset(new base::AutoLock(session_lock_));
  return session_.get();
}

void SessionAccessorImpl::DeleteSession() {
  session_.reset();
}

SessionAccessorImpl::~SessionAccessorImpl() {}
