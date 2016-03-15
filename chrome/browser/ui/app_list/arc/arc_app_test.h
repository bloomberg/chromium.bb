// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace arc {
class AppInfo;
class FakeArcBridgeService;
class FakeAppInstance;
}

namespace content {
class BrowserContext;
}

// Helper class to initialize arc bridge to work with arc apps in unit tests.
class ArcAppTest {
 public:
  ArcAppTest();
  virtual ~ArcAppTest();

  void SetUp(content::BrowserContext* browser_context);

  static std::string GetAppId(const arc::AppInfo& app_info);

  // The 0th item is sticky but not the followings.
  const std::vector<arc::AppInfo>& fake_apps() const { return fake_apps_; }

  arc::FakeArcBridgeService* bridge_service() { return bridge_service_.get(); }

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

 private:
  // Unowned pointer.
  content::BrowserContext* browser_context_ = nullptr;

  scoped_ptr<arc::FakeArcBridgeService> bridge_service_;
  scoped_ptr<arc::FakeAppInstance> app_instance_;
  std::vector<arc::AppInfo> fake_apps_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppTest);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
