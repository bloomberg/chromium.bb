// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_mac.h"

#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart() {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::StartTearDown() {
  app_shim_host_manager_ = NULL;
}

void BrowserProcessPlatformPart::AttemptExit() {
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  chrome_browser_application_mac::Terminate();
}

void BrowserProcessPlatformPart::PreMainMessageLoopRun() {
  // AppShimHostManager can not simply be reset, otherwise destroying the old
  // domain socket will cause the just-created socket to be unlinked.
  DCHECK(!app_shim_host_manager_);
  app_shim_host_manager_ = new AppShimHostManager;
  // Init needs to be called after assigning to a scoped_refptr (i.e. after
  // incrementing the refcount).
  app_shim_host_manager_->Init();
  AppListService::InitAll(NULL);
}

AppShimHostManager* BrowserProcessPlatformPart::app_shim_host_manager() {
  return app_shim_host_manager_.get();
}
