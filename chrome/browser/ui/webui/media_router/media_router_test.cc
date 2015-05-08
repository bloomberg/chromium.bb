// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_test.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/dialog_test_browser_window.h"

MediaRouterTest::MediaRouterTest() {
}

MediaRouterTest::~MediaRouterTest() {
}

void MediaRouterTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableMediaRouter);
}

BrowserWindow* MediaRouterTest::CreateBrowserWindow() {
  return new DialogTestBrowserWindow;
}
