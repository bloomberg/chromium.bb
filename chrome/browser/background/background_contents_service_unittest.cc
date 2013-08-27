// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/message_center/message_center.h"
#include "url/gurl.h"

class BackgroundContentsServiceTest : public testing::Test {
 protected:
  BackgroundContentsServiceTest()
      : command_line_(CommandLine::NO_PROGRAM),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(NULL) {
    CHECK(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("TestProfile");
    service_.reset(new BackgroundContentsService(profile_, &command_line_));
    BackgroundContentsServiceFactory::GetInstance()->
        RegisterUserPrefsOnBrowserContext(profile_);
  }

  virtual ~BackgroundContentsServiceTest() {
    base::RunLoop().RunUntilIdle();
  }

  static void SetUpTestCase() {
    message_center::MessageCenter::Initialize();
  }
  static void TearDownTestCase() {
    message_center::MessageCenter::Shutdown();
  }

  const DictionaryValue* GetPrefs(Profile* profile) {
    return profile->GetPrefs()->GetDictionary(
        prefs::kRegisteredBackgroundContents);
  }

  // Returns the stored pref URL for the passed app id.
  std::string GetPrefURLForApp(Profile* profile, const string16& appid) {
    const DictionaryValue* pref = GetPrefs(profile);
    EXPECT_TRUE(pref->HasKey(UTF16ToUTF8(appid)));
    const DictionaryValue* value;
    pref->GetDictionaryWithoutPathExpansion(UTF16ToUTF8(appid), &value);
    std::string url;
    value->GetString("url", &url);
    return url;
  }

  content::TestBrowserThreadBundle thread_bundle;
  CommandLine command_line_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;  // Not owned.
  scoped_ptr<BackgroundContentsService> service_;
};

class MockBackgroundContents : public BackgroundContents {
 public:
  explicit MockBackgroundContents(Profile* profile)
      : appid_(ASCIIToUTF16("app_id")),
        profile_(profile) {
  }
  MockBackgroundContents(Profile* profile, const std::string& id)
      : appid_(ASCIIToUTF16(id)),
        profile_(profile) {
  }

  void SendOpenedNotification(BackgroundContentsService* service) {
    string16 frame_name = ASCIIToUTF16("background");
    BackgroundContentsOpenedDetails details = {
        this, frame_name, appid_ };
    service->BackgroundContentsOpened(&details);
  }

  virtual void Navigate(GURL url) {
    url_ = url;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
        content::Source<Profile>(profile_),
        content::Details<BackgroundContents>(this));
  }
  virtual const GURL& GetURL() const OVERRIDE { return url_; }

  void MockClose(Profile* profile) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED,
        content::Source<Profile>(profile),
        content::Details<BackgroundContents>(this));
    delete this;
  }

  virtual ~MockBackgroundContents() {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
        content::Source<Profile>(profile_),
        content::Details<BackgroundContents>(this));
  }

  const string16& appid() { return appid_; }

 private:
  GURL url_;

  // The ID of our parent application.
  string16 appid_;

  // Parent profile. Not owned.
  Profile* profile_;
};

TEST_F(BackgroundContentsServiceTest, Create) {
  // Check for creation and leaks when the basic objects in the
  // fixtures are created/destructed.
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsCreateDestroy) {
  MockBackgroundContents* contents = new MockBackgroundContents(profile_);
  EXPECT_FALSE(service_->IsTracked(contents));
  contents->SendOpenedNotification(service_.get());
  EXPECT_TRUE(service_->IsTracked(contents));
  delete contents;
  EXPECT_FALSE(service_->IsTracked(contents));
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAdded) {
  GURL orig_url;
  GURL url("http://a/");
  GURL url2("http://a/");
  {
    scoped_ptr<MockBackgroundContents> contents(
        new MockBackgroundContents(profile_));
    EXPECT_EQ(0U, GetPrefs(profile_)->size());
    contents->SendOpenedNotification(service_.get());

    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(profile_)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));

    // Navigate the contents to a new url, should not change url.
    contents->Navigate(url2);
    EXPECT_EQ(1U, GetPrefs(profile_)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));
  }
  // Contents are deleted, url should persist.
  EXPECT_EQ(1U, GetPrefs(profile_)->size());
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAddedAndClosed) {
  GURL url("http://a/");
  MockBackgroundContents* contents = new MockBackgroundContents(profile_);
  EXPECT_EQ(0U, GetPrefs(profile_)->size());
  contents->SendOpenedNotification(service_.get());
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(profile_)->size());
  EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));

  // Fake a window closed by script.
  contents->MockClose(profile_);
  EXPECT_EQ(0U, GetPrefs(profile_)->size());
}

// Test what happens if a BackgroundContents shuts down (say, due to a renderer
// crash) then is restarted. Should not persist URL twice.
TEST_F(BackgroundContentsServiceTest, RestartBackgroundContents) {
  GURL url("http://a/");
  {
    scoped_ptr<MockBackgroundContents> contents(new MockBackgroundContents(
        profile_, "appid"));
    contents->SendOpenedNotification(service_.get());
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(profile_)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));
  }
  // Contents deleted, url should be persisted.
  EXPECT_EQ(1U, GetPrefs(profile_)->size());

  {
    // Reopen the BackgroundContents to the same URL, we should not register the
    // URL again.
    scoped_ptr<MockBackgroundContents> contents(new MockBackgroundContents(
        profile_, "appid"));
    contents->SendOpenedNotification(service_.get());
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(profile_)->size());
  }
}

// Ensures that BackgroundContentsService properly tracks the association
// between a BackgroundContents and its parent extension, including
// unregistering the BC when the extension is uninstalled.
TEST_F(BackgroundContentsServiceTest, TestApplicationIDLinkage) {
  EXPECT_EQ(NULL, service_->GetAppBackgroundContents(ASCIIToUTF16("appid")));
  MockBackgroundContents* contents = new MockBackgroundContents(profile_,
                                                                "appid");
  scoped_ptr<MockBackgroundContents> contents2(
      new MockBackgroundContents(profile_, "appid2"));
  contents->SendOpenedNotification(service_.get());
  EXPECT_EQ(contents, service_->GetAppBackgroundContents(contents->appid()));
  contents2->SendOpenedNotification(service_.get());
  EXPECT_EQ(contents2.get(), service_->GetAppBackgroundContents(
      contents2->appid()));
  EXPECT_EQ(0U, GetPrefs(profile_)->size());

  // Navigate the contents, then make sure the one associated with the extension
  // is unregistered.
  GURL url("http://a/");
  GURL url2("http://b/");
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(profile_)->size());
  contents2->Navigate(url2);
  EXPECT_EQ(2U, GetPrefs(profile_)->size());
  service_->ShutdownAssociatedBackgroundContents(ASCIIToUTF16("appid"));
  EXPECT_FALSE(service_->IsTracked(contents));
  EXPECT_EQ(NULL, service_->GetAppBackgroundContents(ASCIIToUTF16("appid")));
  EXPECT_EQ(1U, GetPrefs(profile_)->size());
  EXPECT_EQ(url2.spec(), GetPrefURLForApp(profile_, contents2->appid()));
}
