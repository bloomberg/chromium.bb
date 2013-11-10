// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using content::SiteInstance;

namespace extensions {

// TODO(jamescook): Convert this from TestingProfile to TestBrowserContext and
// move to extensions/browser. This is dependent on ExtensionPrefs being
// converted and ExtensionSystem being converted or eliminated.
// http://crbug.com/315855

// make the test a PlatformTest to setup autorelease pools properly on mac
class ProcessManagerTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    ExtensionErrorReporter::Init(false);  // no noisy errors
  }

  virtual void SetUp() {
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }

  // Returns true if the notification |type| is registered for |manager| with
  // source |profile|. Pass NULL for |profile| for all sources.
  static bool IsRegistered(ProcessManager* manager,
                           int type,
                           TestingProfile* profile) {
    return manager->registrar_.IsRegistered(
        manager, type, content::Source<Profile>(profile));
  }
};

// Test that notification registration works properly.
TEST_F(ProcessManagerTest, ExtensionNotificationRegistration) {
  // Test for a normal profile.
  scoped_ptr<TestingProfile> original_profile(new TestingProfile);
  scoped_ptr<ProcessManager> manager1(
      ProcessManager::Create(original_profile.get()));

  EXPECT_EQ(original_profile.get(), manager1->GetBrowserContext());
  EXPECT_EQ(0u, manager1->background_hosts().size());

  // It observes other notifications from this profile.
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSIONS_READY,
                           original_profile.get()));
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSION_LOADED,
                           original_profile.get()));
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSION_UNLOADED,
                           original_profile.get()));
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                           original_profile.get()));

  // Now add an incognito profile associated with the master above.
  TestingProfile::Builder builder;
  builder.SetIncognito();
  scoped_ptr<TestingProfile> incognito_profile = builder.Build();
  incognito_profile->SetOriginalProfile(original_profile.get());
  scoped_ptr<ProcessManager> manager2(
      ProcessManager::Create(incognito_profile.get()));

  EXPECT_EQ(incognito_profile.get(), manager2->GetBrowserContext());
  EXPECT_EQ(0u, manager2->background_hosts().size());

  // Some notifications are observed for the original profile.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_EXTENSION_LOADED,
                           original_profile.get()));

  // Some notifications are observed for the incognito profile.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                           incognito_profile.get()));

  // Some notifications are observed for both incognito and original.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           original_profile.get()));
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           incognito_profile.get()));

  // Some are not observed at all.
  EXPECT_FALSE(IsRegistered(manager2.get(),
                            chrome::NOTIFICATION_EXTENSIONS_READY,
                            original_profile.get()));

  // This notification is observed for incognito profiles only.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           incognito_profile.get()));
}

// Test that extensions get grouped in the right SiteInstance (and therefore
// process) based on their URLs.
TEST_F(ProcessManagerTest, ProcessGrouping) {
  // Extensions in different profiles should always be different SiteInstances.
  // Note: we don't initialize these, since we're not testing that
  // functionality.  This means we can get away with a NULL UserScriptMaster.
  TestingProfile profile1;
  scoped_ptr<ProcessManager> manager1(ProcessManager::Create(&profile1));

  TestingProfile profile2;
  scoped_ptr<ProcessManager> manager2(ProcessManager::Create(&profile2));

  // Extensions with common origins ("scheme://id/") should be grouped in the
  // same SiteInstance.
  GURL ext1_url1("chrome-extension://ext1_id/index.html");
  GURL ext1_url2("chrome-extension://ext1_id/monkey/monkey.html");
  GURL ext2_url1("chrome-extension://ext2_id/index.html");

  scoped_refptr<SiteInstance> site11 =
      manager1->GetSiteInstanceForURL(ext1_url1);
  scoped_refptr<SiteInstance> site12 =
      manager1->GetSiteInstanceForURL(ext1_url2);
  EXPECT_EQ(site11, site12);

  scoped_refptr<SiteInstance> site21 =
      manager1->GetSiteInstanceForURL(ext2_url1);
  EXPECT_NE(site11, site21);

  scoped_refptr<SiteInstance> other_profile_site =
      manager2->GetSiteInstanceForURL(ext1_url1);
  EXPECT_NE(site11, other_profile_site);
}

}  // namespace extensions
