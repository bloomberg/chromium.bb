// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"

#include "base/strings/string_split.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/port_server.h"

ChromeAndroidImpl::ChromeAndroidImpl(
    scoped_ptr<DevToolsHttpClient> http_client,
    scoped_ptr<DevToolsClient> websocket_client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners,
    scoped_ptr<PortReservation> port_reservation,
    scoped_ptr<Device> device)
    : ChromeImpl(http_client.Pass(),
                 websocket_client.Pass(),
                 devtools_event_listeners,
                 port_reservation.Pass()),
      device_(device.Pass()) {}

ChromeAndroidImpl::~ChromeAndroidImpl() {}

Status ChromeAndroidImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation is unsupported on Android");
}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

bool ChromeAndroidImpl::HasTouchScreen() const {
  const BrowserInfo* browser_info = GetBrowserInfo();
  if (browser_info->browser_name == "webview")
    return browser_info->major_version >= 44;
  else
    return browser_info->build_no >= 2388;
}

Status ChromeAndroidImpl::QuitImpl() {
  return device_->TearDown();
}

