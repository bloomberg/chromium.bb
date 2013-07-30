// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"

#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeAndroidImpl::ChromeAndroidImpl(
    scoped_ptr<DevToolsHttpClient> client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners,
    scoped_ptr<Device> device,
    Log* log)
    : ChromeImpl(client.Pass(), devtools_event_listeners, log),
      device_(device.Pass()) {}

ChromeAndroidImpl::~ChromeAndroidImpl() {}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

Status ChromeAndroidImpl::Quit() {
  return device_->StopApp();
}

