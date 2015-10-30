// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_TEST_H_

#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/common/feature_switch.h"

class BrowserWindow;

class MediaRouterTest : public BrowserWithTestWindowTest {
 public:
  MediaRouterTest();
  ~MediaRouterTest() override;

 protected:
  // BrowserWithTestWindowTest override.
  BrowserWindow* CreateBrowserWindow() override;

 private:
  extensions::FeatureSwitch::ScopedOverride feature_override_;
  DISALLOW_COPY_AND_ASSIGN(MediaRouterTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_TEST_H_
