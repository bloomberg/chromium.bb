// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

TEST(WebAppRegistrar, CreateRegisterUnregister) {
  auto registrar = std::make_unique<WebAppRegistrar>();
  EXPECT_EQ(nullptr, registrar->GetAppById(AppId()));

  const GURL launch_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(launch_url);
  const std::string name = "Name";
  const std::string description = "Description";

  const GURL launch_url2 = GURL("https://example.com/path2");
  const AppId app_id2 = GenerateAppIdFromURL(launch_url2);

  auto web_app = std::make_unique<WebApp>(app_id);
  auto web_app2 = std::make_unique<WebApp>(app_id2);

  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetLaunchUrl(launch_url.spec());

  EXPECT_EQ(nullptr, registrar->GetAppById(app_id));
  EXPECT_EQ(nullptr, registrar->GetAppById(app_id2));

  registrar->RegisterApp(std::move(web_app));
  WebApp* app = registrar->GetAppById(app_id);

  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(name, app->name());
  EXPECT_EQ(description, app->description());
  EXPECT_EQ(launch_url.spec(), app->launch_url());

  EXPECT_EQ(nullptr, registrar->GetAppById(app_id2));

  registrar->RegisterApp(std::move(web_app2));
  WebApp* app2 = registrar->GetAppById(app_id2);
  EXPECT_EQ(app_id2, app2->app_id());

  registrar->UnregisterApp(app_id);
  EXPECT_EQ(nullptr, registrar->GetAppById(app_id));

  // Check that app2 is still registered.
  app2 = registrar->GetAppById(app_id2);
  EXPECT_EQ(app_id2, app2->app_id());

  registrar->UnregisterApp(app_id2);
  EXPECT_EQ(nullptr, registrar->GetAppById(app_id2));
}

TEST(WebAppRegistrar, DestroyRegistrarOwningRegisteredApps) {
  auto registrar = std::make_unique<WebAppRegistrar>();

  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/path"));
  const AppId app_id2 = GenerateAppIdFromURL(GURL("https://example.com/path2"));

  auto web_app = std::make_unique<WebApp>(app_id);
  registrar->RegisterApp(std::move(web_app));

  auto web_app2 = std::make_unique<WebApp>(app_id2);
  registrar->RegisterApp(std::move(web_app2));

  registrar.reset();
}

}  // namespace web_app
