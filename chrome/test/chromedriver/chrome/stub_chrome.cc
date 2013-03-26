// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

StubChrome::StubChrome() {}

StubChrome::~StubChrome() {}

std::string StubChrome::GetVersion() {
  return "";
}

Status StubChrome::GetWebViewIds(std::list<std::string>* web_view_ids) {
  return Status(kOk);
}

Status StubChrome::GetWebViewById(const std::string& id, WebView** web_view) {
  return Status(kOk);
}

Status StubChrome::IsJavaScriptDialogOpen(bool* is_open) {
  return Status(kOk);
}

Status StubChrome::GetJavaScriptDialogMessage(std::string* message) {
  return Status(kOk);
}

Status StubChrome::HandleJavaScriptDialog(
    bool accept,
    const std::string& prompt_text) {
  return Status(kOk);
}

std::string StubChrome::GetOperatingSystemName() {
  return "";
}

Status StubChrome::Quit() {
  return Status(kOk);
}
