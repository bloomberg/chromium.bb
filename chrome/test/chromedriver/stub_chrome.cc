// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/stub_chrome.h"
#include "chrome/test/chromedriver/status.h"

StubChrome::StubChrome() {}

StubChrome::~StubChrome() {}

Status StubChrome::Load(const std::string& url) {
  return Status(kOk);
}

Status StubChrome::Reload() {
  return Status(kOk);
}

Status StubChrome::EvaluateScript(const std::string& frame,
                                  const std::string& function,
                                  scoped_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubChrome::CallFunction(const std::string& frame,
                                const std::string& function,
                                const base::ListValue& args,
                                scoped_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubChrome::GetFrameByFunction(const std::string& frame,
                                      const std::string& function,
                                      const base::ListValue& args,
                                      std::string* out_frame) {
  return Status(kOk);
}

Status StubChrome::DispatchMouseEvents(
    const std::list<MouseEvent>& events) {
  return Status(kOk);
}

Status StubChrome::DispatchKeyEvents(
    const std::list<KeyEvent>& events) {
  return Status(kOk);
}

Status StubChrome::Quit() {
  return Status(kOk);
}

Status StubChrome::WaitForPendingNavigations(
    const std::string& frame_id) {
  return Status(kOk);
}

Status StubChrome::GetMainFrame(
    std::string* frame_id) {
  return Status(kOk);
}
