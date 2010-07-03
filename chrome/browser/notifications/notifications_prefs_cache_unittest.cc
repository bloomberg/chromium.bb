// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifications_prefs_cache.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPresenter.h"

TEST(NotificationsPrefsCacheTest, CanCreate) {
  scoped_refptr<NotificationsPrefsCache> cache(new NotificationsPrefsCache());
  std::vector<GURL> allowed_origins;
  allowed_origins.push_back(GURL("http://allowed.com"));
  std::vector<GURL> denied_origins;
  denied_origins.push_back(GURL("http://denied.com"));

  {
    MessageLoop loop;
    ChromeThread ui_thread(ChromeThread::UI, &loop);

    cache->SetCacheAllowedOrigins(allowed_origins);
    cache->SetCacheDeniedOrigins(denied_origins);
    cache->SetCacheDefaultContentSetting(CONTENT_SETTING_DEFAULT);
  }

  cache->set_is_initialized(true);

  {
    MessageLoop loop;
    ChromeThread io_thread(ChromeThread::IO, &loop);

    cache->CacheAllowedOrigin(GURL("http://allowed2.com"));
    cache->CacheDeniedOrigin(GURL("http://denied2.com"));

    EXPECT_EQ(cache->HasPermission(GURL("http://allowed.com")),
              WebKit::WebNotificationPresenter::PermissionAllowed);
    EXPECT_EQ(cache->HasPermission(GURL("http://allowed2.com")),
              WebKit::WebNotificationPresenter::PermissionAllowed);

    EXPECT_EQ(cache->HasPermission(GURL("http://denied.com")),
              WebKit::WebNotificationPresenter::PermissionDenied);
    EXPECT_EQ(cache->HasPermission(GURL("http://denied2.com")),
              WebKit::WebNotificationPresenter::PermissionDenied);

    EXPECT_EQ(cache->HasPermission(GURL("http://unkown.com")),
              WebKit::WebNotificationPresenter::PermissionNotAllowed);

    cache->SetCacheDefaultContentSetting(CONTENT_SETTING_ASK);
    EXPECT_EQ(cache->HasPermission(GURL("http://unkown.com")),
              WebKit::WebNotificationPresenter::PermissionNotAllowed);

    cache->SetCacheDefaultContentSetting(CONTENT_SETTING_ALLOW);
    EXPECT_EQ(cache->HasPermission(GURL("http://unkown.com")),
              WebKit::WebNotificationPresenter::PermissionAllowed);

    cache->SetCacheDefaultContentSetting(CONTENT_SETTING_BLOCK);
    EXPECT_EQ(cache->HasPermission(GURL("http://unkown.com")),
              WebKit::WebNotificationPresenter::PermissionDenied);
  }
}

