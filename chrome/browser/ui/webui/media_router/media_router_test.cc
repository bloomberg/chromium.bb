// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_test.h"

#include "chrome/test/base/dialog_test_browser_window.h"

MediaRouterTest::MediaRouterTest()
    : feature_override_(extensions::FeatureSwitch::media_router(), true) {}

MediaRouterTest::~MediaRouterTest() {
}

BrowserWindow* MediaRouterTest::CreateBrowserWindow() {
  return new DialogTestBrowserWindow;
}
