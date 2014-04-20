// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::SiteInstance;
using content::TestBrowserContext;

namespace extensions {

namespace {

// An incognito version of a TestBrowserContext.
class TestBrowserContextIncognito : public TestBrowserContext {
 public:
  TestBrowserContextIncognito() {}
  virtual ~TestBrowserContextIncognito() {}

  // TestBrowserContext implementation.
  virtual bool IsOffTheRecord() const OVERRIDE { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserContextIncognito);
};

}  // namespace

class ProcessManagerTest : public testing::Test {
 public:
  ProcessManagerTest() : extensions_browser_client_(&original_context_) {
    extensions_browser_client_.SetIncognitoContext(&incognito_context_);
    ExtensionsBrowserClient::Set(&extensions_browser_client_);
  }

  virtual ~ProcessManagerTest() {
    ExtensionsBrowserClient::Set(NULL);
  }

  BrowserContext* original_context() { return &original_context_; }
  BrowserContext* incognito_context() { return &incognito_context_; }

  // Returns true if the notification |type| is registered for |manager| with
  // source |context|. Pass NULL for |context| for all sources.
  static bool IsRegistered(ProcessManager* manager,
                           int type,
                           BrowserContext* context) {
    return manager->registrar_.IsRegistered(
        manager, type, content::Source<BrowserContext>(context));
  }

 private:
  TestBrowserContext original_context_;
  TestBrowserContextIncognito incognito_context_;
  TestExtensionsBrowserClient extensions_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(ProcessManagerTest);
};

// Test that notification registration works properly.
TEST_F(ProcessManagerTest, ExtensionNotificationRegistration) {
  // Test for a normal context ProcessManager.
  scoped_ptr<ProcessManager> manager1(
      ProcessManager::Create(original_context()));

  EXPECT_EQ(original_context(), manager1->GetBrowserContext());
  EXPECT_EQ(0u, manager1->background_hosts().size());

  // It observes other notifications from this context.
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSIONS_READY,
                           original_context()));
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                           original_context()));
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                           original_context()));
  EXPECT_TRUE(IsRegistered(manager1.get(),
                           chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                           original_context()));

  // Test for an incognito context ProcessManager.
  scoped_ptr<ProcessManager> manager2(ProcessManager::CreateIncognitoForTesting(
      incognito_context(), original_context(), manager1.get()));

  EXPECT_EQ(incognito_context(), manager2->GetBrowserContext());
  EXPECT_EQ(0u, manager2->background_hosts().size());

  // Some notifications are observed for the original context.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                           original_context()));

  // Some notifications are observed for the incognito context.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                           incognito_context()));

  // Some notifications are observed for both incognito and original.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           original_context()));
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           incognito_context()));

  // Some are not observed at all.
  EXPECT_FALSE(IsRegistered(manager2.get(),
                            chrome::NOTIFICATION_EXTENSIONS_READY,
                            original_context()));

  // This notification is observed for incognito contexts only.
  EXPECT_TRUE(IsRegistered(manager2.get(),
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           incognito_context()));
}

// Test that extensions get grouped in the right SiteInstance (and therefore
// process) based on their URLs.
TEST_F(ProcessManagerTest, ProcessGrouping) {
  content::ContentBrowserClient content_browser_client;
  content::SetBrowserClientForTesting(&content_browser_client);

  // Extensions in different browser contexts should always be different
  // SiteInstances.
  scoped_ptr<ProcessManager> manager1(
      ProcessManager::Create(original_context()));
  // NOTE: This context is not associated with the TestExtensionsBrowserClient.
  // That's OK because we're not testing regular vs. incognito behavior.
  TestBrowserContext another_context;
  scoped_ptr<ProcessManager> manager2(ProcessManager::Create(&another_context));

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
