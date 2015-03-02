// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_browser_state.h"

#include "base/files/file_path.h"

namespace web {
TestBrowserState::TestBrowserState() {
}

TestBrowserState::~TestBrowserState() {
}

bool TestBrowserState::IsOffTheRecord() const {
  return false;
}

base::FilePath TestBrowserState::GetStatePath() const {
  return base::FilePath();
}

net::URLRequestContextGetter* TestBrowserState::GetRequestContext() {
  return nullptr;
}
}  // namespace web
