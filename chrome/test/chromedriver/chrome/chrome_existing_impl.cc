// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_existing_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeExistingImpl::ChromeExistingImpl(
    scoped_ptr<DevToolsHttpClient> client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners,
    Log* log)
    : ChromeImpl(client.Pass(), devtools_event_listeners, log) {}

ChromeExistingImpl::~ChromeExistingImpl() {}

std::string ChromeExistingImpl::GetOperatingSystemName() {
 return std::string();
}

Status ChromeExistingImpl::Quit() {
  return Status(kOk);
}

