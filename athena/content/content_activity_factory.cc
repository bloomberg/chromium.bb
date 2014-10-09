// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_activity_factory.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity.h"
#include "athena/content/web_activity.h"
#include "base/logging.h"
#include "ui/aura/window.h"

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
  activity->GetWindow()->SetName("WebActivity");
  return activity;
}

Activity* ContentActivityFactory::CreateWebActivity(
    content::WebContents* contents) {
  Activity* activity = new WebActivity(contents);
  ActivityManager::Get()->AddActivity(activity);
  return activity;
}

Activity* ContentActivityFactory::CreateAppActivity(
    const std::string& app_id,
    views::WebView* web_view) {
  Activity* activity = new AppActivity(app_id, web_view);
  ActivityManager::Get()->AddActivity(activity);
  activity->GetWindow()->SetName("AppActivity");
  return activity;
}

ActivityFactory* CreateContentActivityFactory() {
  return new ContentActivityFactory();
}

}  // namespace athena
