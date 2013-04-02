// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"

#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeAndroidImpl::ChromeAndroidImpl(
    scoped_ptr<DevToolsHttpClient> client,
    const std::string& version,
    int build_no)
    : ChromeImpl(client.Pass(), version, build_no) {}

ChromeAndroidImpl::~ChromeAndroidImpl() {}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

Status ChromeAndroidImpl::Quit() {
  // NOOP.
  return Status(kOk);
}

