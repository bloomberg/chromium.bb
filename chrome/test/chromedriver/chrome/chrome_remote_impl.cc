// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_remote_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/port_server.h"

ChromeRemoteImpl::ChromeRemoteImpl(
    scoped_ptr<DevToolsHttpClient> http_client,
    scoped_ptr<DevToolsClient> websocket_client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners)
    : ChromeImpl(http_client.Pass(),
                 websocket_client.Pass(),
                 devtools_event_listeners,
                 scoped_ptr<PortReservation>()) {}

ChromeRemoteImpl::~ChromeRemoteImpl() {}

std::string ChromeRemoteImpl::GetOperatingSystemName() {
 return std::string();
}

Status ChromeRemoteImpl::QuitImpl() {
  return Status(kOk);
}

