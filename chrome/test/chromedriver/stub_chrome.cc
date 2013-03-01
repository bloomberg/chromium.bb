// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/stub_chrome.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/web_view.h"

StubChrome::StubChrome() {}

StubChrome::~StubChrome() {}

std::string StubChrome::GetVersion() {
  return "";
}

Status StubChrome::GetWebViews(std::list<WebView*>* web_views) {
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

Status StubChrome::Quit() {
  return Status(kOk);
}
