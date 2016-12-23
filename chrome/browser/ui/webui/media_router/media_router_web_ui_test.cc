// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_web_ui_test.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/router/media_router_ui_service.h"
#include "chrome/browser/media/router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/toolbar/mock_media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/test/base/dialog_test_browser_window.h"

class MockMediaRouterUIService : public media_router::MediaRouterUIService {
 public:
  explicit MockMediaRouterUIService(Profile* profile)
      : media_router::MediaRouterUIService(profile),
        action_controller_(profile) {}
  ~MockMediaRouterUIService() override {}

  MediaRouterActionController* action_controller() override {
    return &action_controller_;
  }

 private:
  MockMediaRouterActionController action_controller_;
};

std::unique_ptr<KeyedService> BuildMockMediaRouterUIService(
    content::BrowserContext* context) {
  return base::MakeUnique<MockMediaRouterUIService>(
      static_cast<Profile*>(context));
}

std::unique_ptr<KeyedService> BuildToolbarActionsModel(
    content::BrowserContext* context) {
  return base::MakeUnique<ToolbarActionsModel>(static_cast<Profile*>(context),
                                               nullptr);
}

MediaRouterWebUITest::MediaRouterWebUITest() : MediaRouterWebUITest(false) {}
MediaRouterWebUITest::MediaRouterWebUITest(bool require_mock_ui_service)
    : require_mock_ui_service_(require_mock_ui_service) {}

MediaRouterWebUITest::~MediaRouterWebUITest() {}

TestingProfile* MediaRouterWebUITest::CreateProfile() {
  TestingProfile::Builder builder;
  if (require_mock_ui_service_) {
    builder.AddTestingFactory(
        media_router::MediaRouterUIServiceFactory::GetInstance(),
        BuildMockMediaRouterUIService);
    builder.AddTestingFactory(ToolbarActionsModelFactory::GetInstance(),
                              BuildToolbarActionsModel);
  }
  return builder.Build().release();
}

BrowserWindow* MediaRouterWebUITest::CreateBrowserWindow() {
  return new DialogTestBrowserWindow;
}
