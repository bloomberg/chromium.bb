// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionSet;
using extensions::Manifest;
using storage::SpecialStoragePolicy;

typedef SpecialStoragePolicy::StoragePolicy StoragePolicy;

namespace keys = extensions::manifest_keys;

class ExtensionSpecialStoragePolicyTest : public testing::Test {
 protected:
  class PolicyChangeObserver : public SpecialStoragePolicy::Observer {
   public:
    PolicyChangeObserver()
        : expected_type_(NOTIFICATION_TYPE_NONE),
          expected_change_flags_(0) {
    }

    virtual void OnGranted(const GURL& origin,
                           int change_flags) OVERRIDE {
      EXPECT_EQ(expected_type_, NOTIFICATION_TYPE_GRANT);
      EXPECT_EQ(expected_origin_, origin);
      EXPECT_EQ(expected_change_flags_, change_flags);
      expected_type_ = NOTIFICATION_TYPE_NONE;
    }

    virtual void OnRevoked(const GURL& origin,
                           int change_flags) OVERRIDE {
      EXPECT_EQ(expected_type_, NOTIFICATION_TYPE_REVOKE);
      EXPECT_EQ(expected_origin_, origin);
      EXPECT_EQ(expected_change_flags_, change_flags);
      expected_type_ = NOTIFICATION_TYPE_NONE;
    }

    virtual void OnCleared() OVERRIDE {
      EXPECT_EQ(expected_type_, NOTIFICATION_TYPE_CLEAR);
      expected_type_ = NOTIFICATION_TYPE_NONE;
    }

    void ExpectGrant(const std::string& extension_id,
                     int change_flags) {
      expected_type_ = NOTIFICATION_TYPE_GRANT;
      expected_origin_ = Extension::GetBaseURLFromExtensionId(extension_id);
      expected_change_flags_ = change_flags;
    }

    void ExpectRevoke(const std::string& extension_id,
                      int change_flags) {
      expected_type_ = NOTIFICATION_TYPE_REVOKE;
      expected_origin_ = Extension::GetBaseURLFromExtensionId(extension_id);
      expected_change_flags_ = change_flags;
    }

    void ExpectClear() {
      expected_type_ = NOTIFICATION_TYPE_CLEAR;
    }

    bool IsCompleted() {
      return expected_type_ == NOTIFICATION_TYPE_NONE;
    }

   private:
    enum {
      NOTIFICATION_TYPE_NONE,
      NOTIFICATION_TYPE_GRANT,
      NOTIFICATION_TYPE_REVOKE,
      NOTIFICATION_TYPE_CLEAR,
    } expected_type_;

    GURL expected_origin_;
    int expected_change_flags_;

    DISALLOW_COPY_AND_ASSIGN(PolicyChangeObserver);
  };

  virtual void SetUp() OVERRIDE {
    policy_ = new ExtensionSpecialStoragePolicy(NULL);
  }

  scoped_refptr<Extension> CreateProtectedApp() {
#if defined(OS_WIN)
    base::FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
    base::FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
    base::DictionaryValue manifest;
    manifest.SetString(keys::kName, "Protected");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kLaunchWebURL, "http://explicit/protected/start");
    base::ListValue* list = new base::ListValue();
    list->Append(new base::StringValue("http://explicit/protected"));
    list->Append(new base::StringValue("*://*.wildcards/protected"));
    manifest.Set(keys::kWebURLs, list);
    std::string error;
    scoped_refptr<Extension> protected_app = Extension::Create(
        path, Manifest::INVALID_LOCATION, manifest,
        Extension::NO_FLAGS, &error);
    EXPECT_TRUE(protected_app.get()) << error;
    return protected_app;
  }

  scoped_refptr<Extension> CreateUnlimitedApp() {
#if defined(OS_WIN)
    base::FilePath path(FILE_PATH_LITERAL("c:\\bar"));
#elif defined(OS_POSIX)
    base::FilePath path(FILE_PATH_LITERAL("/bar"));
#endif
    base::DictionaryValue manifest;
    manifest.SetString(keys::kName, "Unlimited");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kLaunchWebURL, "http://explicit/unlimited/start");
    base::ListValue* list = new base::ListValue();
    list->Append(new base::StringValue("unlimitedStorage"));
    manifest.Set(keys::kPermissions, list);
    list = new base::ListValue();
    list->Append(new base::StringValue("http://explicit/unlimited"));
    list->Append(new base::StringValue("*://*.wildcards/unlimited"));
    manifest.Set(keys::kWebURLs, list);
    std::string error;
    scoped_refptr<Extension> unlimited_app = Extension::Create(
        path, Manifest::INVALID_LOCATION, manifest,
        Extension::NO_FLAGS, &error);
    EXPECT_TRUE(unlimited_app.get()) << error;
    return unlimited_app;
  }

  scoped_refptr<Extension> CreateRegularApp() {
#if defined(OS_WIN)
    base::FilePath path(FILE_PATH_LITERAL("c:\\app"));
#elif defined(OS_POSIX)
    base::FilePath path(FILE_PATH_LITERAL("/app"));
#endif
    base::DictionaryValue manifest;
    manifest.SetString(keys::kName, "App");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kPlatformAppBackgroundPage, "background.html");
    std::string error;
    scoped_refptr<Extension> app = Extension::Create(
        path, Manifest::INVALID_LOCATION, manifest,
        Extension::NO_FLAGS, &error);
    EXPECT_TRUE(app.get()) << error;
    return app;
  }

  // Verifies that the set of extensions protecting |url| is *exactly* equal to
  // |expected_extensions|. Pass in an empty set to verify that an origin is not
  // protected.
  void ExpectProtectedBy(const ExtensionSet& expected_extensions,
                         const GURL& url) {
    const ExtensionSet* extensions = policy_->ExtensionsProtectingOrigin(url);
    EXPECT_EQ(expected_extensions.size(), extensions->size());
    for (ExtensionSet::const_iterator it = expected_extensions.begin();
         it != expected_extensions.end(); ++it) {
      EXPECT_TRUE(extensions->Contains((*it)->id()))
          << "Origin " << url << "not protected by extension ID "
          << (*it)->id();
    }
  }

  scoped_refptr<ExtensionSpecialStoragePolicy> policy_;
};

TEST_F(ExtensionSpecialStoragePolicyTest, EmptyPolicy) {
  const GURL kHttpUrl("http://foo");
  const GURL kExtensionUrl("chrome-extension://bar");
  scoped_refptr<Extension> app(CreateRegularApp());

  EXPECT_FALSE(policy_->IsStorageUnlimited(kHttpUrl));
  EXPECT_FALSE(policy_->IsStorageUnlimited(kHttpUrl));  // test cached result
  EXPECT_FALSE(policy_->IsStorageUnlimited(kExtensionUrl));
  EXPECT_FALSE(policy_->IsStorageUnlimited(app->url()));
  ExtensionSet empty_set;
  ExpectProtectedBy(empty_set, kHttpUrl);

  // This one is just based on the scheme.
  EXPECT_TRUE(policy_->IsStorageProtected(kExtensionUrl));
  EXPECT_TRUE(policy_->IsStorageProtected(app->url()));
}

TEST_F(ExtensionSpecialStoragePolicyTest, AppWithProtectedStorage) {
  scoped_refptr<Extension> extension(CreateProtectedApp());
  policy_->GrantRightsForExtension(extension.get());
  ExtensionSet protecting_extensions;
  protecting_extensions.Insert(extension);
  ExtensionSet empty_set;

  EXPECT_FALSE(policy_->IsStorageUnlimited(extension->url()));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  ExpectProtectedBy(protecting_extensions, GURL("http://explicit/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://explicit:6000/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://bar.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("http://not_listed/"));

  policy_->RevokeRightsForExtension(extension.get());
  ExpectProtectedBy(empty_set, GURL("http://explicit/"));
  ExpectProtectedBy(empty_set, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("https://bar.wildcards/"));
}

TEST_F(ExtensionSpecialStoragePolicyTest, AppWithUnlimitedStorage) {
  scoped_refptr<Extension> extension(CreateUnlimitedApp());
  policy_->GrantRightsForExtension(extension.get());
  ExtensionSet protecting_extensions;
  protecting_extensions.Insert(extension);
  ExtensionSet empty_set;

  ExpectProtectedBy(protecting_extensions, GURL("http://explicit/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://explicit:6000/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://bar.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("http://not_listed/"));
  EXPECT_TRUE(policy_->IsStorageUnlimited(extension->url()));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://not_listed/")));

  policy_->RevokeRightsForExtension(extension.get());
  ExpectProtectedBy(empty_set, GURL("http://explicit/"));
  ExpectProtectedBy(empty_set, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("http://bar.wildcards/"));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://bar.wildcards/")));
}

TEST_F(ExtensionSpecialStoragePolicyTest, CanQueryDiskSize) {
  const GURL kHttpUrl("http://foo");
  const GURL kExtensionUrl("chrome-extension://bar");
  scoped_refptr<Extension> regular_app(CreateRegularApp());
  scoped_refptr<Extension> protected_app(CreateProtectedApp());
  scoped_refptr<Extension> unlimited_app(CreateUnlimitedApp());
  policy_->GrantRightsForExtension(regular_app.get());
  policy_->GrantRightsForExtension(protected_app.get());
  policy_->GrantRightsForExtension(unlimited_app.get());

  EXPECT_FALSE(policy_->CanQueryDiskSize(kHttpUrl));
  EXPECT_FALSE(policy_->CanQueryDiskSize(kExtensionUrl));
  EXPECT_TRUE(policy_->CanQueryDiskSize(regular_app->url()));
  EXPECT_TRUE(policy_->CanQueryDiskSize(protected_app->url()));
  EXPECT_TRUE(policy_->CanQueryDiskSize(unlimited_app->url()));
}

TEST_F(ExtensionSpecialStoragePolicyTest, HasIsolatedStorage) {
  const GURL kHttpUrl("http://foo");
  const GURL kExtensionUrl("chrome-extension://bar");
  scoped_refptr<Extension> app(CreateRegularApp());
  policy_->GrantRightsForExtension(app.get());

  EXPECT_FALSE(policy_->HasIsolatedStorage(kHttpUrl));
  EXPECT_FALSE(policy_->HasIsolatedStorage(kExtensionUrl));
  EXPECT_TRUE(policy_->HasIsolatedStorage(app->url()));
}

TEST_F(ExtensionSpecialStoragePolicyTest, OverlappingApps) {
  scoped_refptr<Extension> protected_app(CreateProtectedApp());
  scoped_refptr<Extension> unlimited_app(CreateUnlimitedApp());
  policy_->GrantRightsForExtension(protected_app.get());
  policy_->GrantRightsForExtension(unlimited_app.get());
  ExtensionSet protecting_extensions;
  ExtensionSet empty_set;
  protecting_extensions.Insert(protected_app);
  protecting_extensions.Insert(unlimited_app);

  ExpectProtectedBy(protecting_extensions, GURL("http://explicit/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://explicit:6000/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://bar.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("http://not_listed/"));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy_->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://not_listed/")));

  policy_->RevokeRightsForExtension(unlimited_app.get());
  protecting_extensions.Remove(unlimited_app->id());
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  ExpectProtectedBy(protecting_extensions, GURL("http://explicit/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://bar.wildcards/"));

  policy_->RevokeRightsForExtension(protected_app.get());
  ExpectProtectedBy(empty_set, GURL("http://explicit/"));
  ExpectProtectedBy(empty_set, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("https://bar.wildcards/"));
}

TEST_F(ExtensionSpecialStoragePolicyTest, HasSessionOnlyOrigins) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  TestingProfile profile;
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(&profile).get();
  policy_ = new ExtensionSpecialStoragePolicy(cookie_settings);

  EXPECT_FALSE(policy_->HasSessionOnlyOrigins());

  // The default setting can be session-only.
  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(policy_->HasSessionOnlyOrigins());

  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(policy_->HasSessionOnlyOrigins());

  // Or the session-onlyness can affect individual origins.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("pattern.com");

  cookie_settings->SetCookieSetting(pattern,
                                    ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTING_SESSION_ONLY);

  EXPECT_TRUE(policy_->HasSessionOnlyOrigins());

  // Clearing an origin-specific rule.
  cookie_settings->ResetCookieSetting(pattern,
                                      ContentSettingsPattern::Wildcard());

  EXPECT_FALSE(policy_->HasSessionOnlyOrigins());
}

TEST_F(ExtensionSpecialStoragePolicyTest, NotificationTest) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread io_thread(BrowserThread::IO, &message_loop);

  PolicyChangeObserver observer;
  policy_->AddObserver(&observer);

  scoped_refptr<Extension> apps[] = {
    CreateProtectedApp(),
    CreateUnlimitedApp(),
  };

  int change_flags[] = {
    SpecialStoragePolicy::STORAGE_PROTECTED,

    SpecialStoragePolicy::STORAGE_PROTECTED |
    SpecialStoragePolicy::STORAGE_UNLIMITED,
  };

  ASSERT_EQ(arraysize(apps), arraysize(change_flags));
  for (size_t i = 0; i < arraysize(apps); ++i) {
    SCOPED_TRACE(testing::Message() << "i: " << i);
    observer.ExpectGrant(apps[i]->id(), change_flags[i]);
    policy_->GrantRightsForExtension(apps[i].get());
    message_loop.RunUntilIdle();
    EXPECT_TRUE(observer.IsCompleted());
  }

  for (size_t i = 0; i < arraysize(apps); ++i) {
    SCOPED_TRACE(testing::Message() << "i: " << i);
    policy_->GrantRightsForExtension(apps[i].get());
    message_loop.RunUntilIdle();
    EXPECT_TRUE(observer.IsCompleted());
  }

  for (size_t i = 0; i < arraysize(apps); ++i) {
    SCOPED_TRACE(testing::Message() << "i: " << i);
    observer.ExpectRevoke(apps[i]->id(), change_flags[i]);
    policy_->RevokeRightsForExtension(apps[i].get());
    message_loop.RunUntilIdle();
    EXPECT_TRUE(observer.IsCompleted());
  }

  for (size_t i = 0; i < arraysize(apps); ++i) {
    SCOPED_TRACE(testing::Message() << "i: " << i);
    policy_->RevokeRightsForExtension(apps[i].get());
    message_loop.RunUntilIdle();
    EXPECT_TRUE(observer.IsCompleted());
  }

  observer.ExpectClear();
  policy_->RevokeRightsForAllExtensions();
  message_loop.RunUntilIdle();
  EXPECT_TRUE(observer.IsCompleted());

  policy_->RemoveObserver(&observer);
}
