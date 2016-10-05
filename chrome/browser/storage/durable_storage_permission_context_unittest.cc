// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;

namespace {

void DoNothing(ContentSetting content_setting) {}

class TestDurablePermissionContext : public DurableStoragePermissionContext {
 public:
  explicit TestDurablePermissionContext(Profile* profile)
      : DurableStoragePermissionContext(profile),
        permission_set_count_(0),
        last_permission_set_persisted_(false),
        last_permission_set_setting_(CONTENT_SETTING_DEFAULT) {}

  int permission_set_count() const { return permission_set_count_; }
  bool last_permission_set_persisted() const {
    return last_permission_set_persisted_;
  }
  ContentSetting last_permission_set_setting() const {
    return last_permission_set_setting_;
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    return HostContentSettingsMapFactory::GetForProfile(profile())
        ->GetContentSetting(url_a.GetOrigin(), url_b.GetOrigin(),
                            CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
                            std::string());
  }

 private:
  // NotificationPermissionContext:
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           ContentSetting content_setting) override {
    permission_set_count_++;
    last_permission_set_persisted_ = persist;
    last_permission_set_setting_ = content_setting;
    DurableStoragePermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedder_origin, callback, persist,
        content_setting);
  }

  int permission_set_count_;
  bool last_permission_set_persisted_;
  ContentSetting last_permission_set_setting_;
};

}  // namespace

class BookmarksOriginTest : public ::testing::Test {
 protected:
  static std::vector<BookmarkModel::URLAndTitle> MakeBookmarks(
      const std::string urls[],
      const int array_size) {
    std::vector<BookmarkModel::URLAndTitle> bookmarks;
    for (int i = 0; i < array_size; ++i) {
      BookmarkModel::URLAndTitle bookmark;
      bookmark.url = GURL(urls[i]);
      EXPECT_TRUE(bookmark.url.is_valid());
      bookmarks.push_back(bookmark);
    }
    return bookmarks;
  }
};

TEST_F(BookmarksOriginTest, Exists) {
  std::string urls[] = {
    "http://www.google.com/",
    "https://dogs.com/somepage.html",
    "https://mail.google.com/mail/u/0/#inbox",
  };
  std::vector<BookmarkModel::URLAndTitle> bookmarks =
      MakeBookmarks(urls, arraysize(urls));
  GURL looking_for("https://dogs.com");
  EXPECT_TRUE(DurableStoragePermissionContext::IsOriginBookmarked(
      bookmarks, looking_for));
}

TEST_F(BookmarksOriginTest, DoesntExist) {
  std::string urls[] = {
    "http://www.google.com/",
    "https://www.google.com/",
  };
  std::vector<BookmarkModel::URLAndTitle> bookmarks =
      MakeBookmarks(urls, arraysize(urls));
  GURL looking_for("https://dogs.com");
  EXPECT_FALSE(DurableStoragePermissionContext::IsOriginBookmarked(
      bookmarks, looking_for));
}

class DurableStoragePermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE);
  }

  void AddBookmark(const GURL& origin) {
    if (!model_) {
      profile()->CreateBookmarkModel(true);
      model_ = BookmarkModelFactory::GetForBrowserContext(profile());
      bookmarks::test::WaitForBookmarkModelToLoad(model_);
    }

    model_->AddURL(model_->bookmark_bar_node(), 0,
                   base::ASCIIToUTF16(origin.spec()), origin);
  }

  BookmarkModel* model_ = nullptr;
};

TEST_F(DurableStoragePermissionContextTest, Bookmarked) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  AddBookmark(url);
  NavigateAndCommit(url);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(web_contents(), id, url, url,
                                      true /* user_gesture */,
                                      base::Bind(&DoNothing));
  // Success.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, BookmarkAndIncognitoMode) {
  TestDurablePermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.google.com");
  AddBookmark(url);
  NavigateAndCommit(url);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(web_contents(), id, url, url,
                                      true /* user_gesture */,
                                      base::Bind(&DoNothing));
  // Success.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, NoBookmark) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  NavigateAndCommit(url);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(web_contents(), id, url, url,
                                      true /* user_gesture */,
                                      base::Bind(&DoNothing));

  // We shouldn't be granted.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_FALSE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, CookiesNotAllowed) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  AddBookmark(url);
  NavigateAndCommit(url);

  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile());

  cookie_settings->SetCookieSetting(url, CONTENT_SETTING_BLOCK);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(web_contents(), id, url, url,
                                      true /* user_gesture */,
                                      base::Bind(&DoNothing));
  // We shouldn't be granted.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_FALSE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, EmbeddedFrame) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  GURL requesting_url("https://www.youtube.com");
  AddBookmark(url);
  NavigateAndCommit(url);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(web_contents(), id, requesting_url, url,
                                      true /* user_gesture */,
                                      base::Bind(&DoNothing));
  // We shouldn't be granted.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_FALSE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, NonsecureOrigin) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("http://www.google.com");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetPermissionStatus(url, url));
}
