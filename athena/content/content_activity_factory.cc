// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_activity_factory.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity.h"
#include "athena/content/web_activity.h"
#include "base/logging.h"

namespace athena {

ContentActivityFactory::ContentActivityFactory() {
}

ContentActivityFactory::~ContentActivityFactory() {}

Activity* ContentActivityFactory::CreateWebActivity(
    content::BrowserContext* browser_context,
    const base::string16& title,
    const GURL& url) {
  Activity* activity = new WebActivity(browser_context, title, url);
  ActivityManager::Get()->AddActivity(activity);
  return activity;
}

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::AppWindow* app_window,
    views::WebView* web_view) {
  Activity* activity = new AppActivity(app_window, web_view);
  ActivityManager::Get()->AddActivity(activity);
  return activity;
}

ActivityFactory* CreateContentActivityFactory() {
  return new ContentActivityFactory();
}

}  // namespace athena
