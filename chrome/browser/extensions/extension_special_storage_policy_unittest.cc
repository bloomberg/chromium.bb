// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/web_intents_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::Extension;

namespace keys = extension_manifest_keys;

class ExtensionSpecialStoragePolicyTest : public testing::Test {
 public:
  virtual void SetUp() {
    policy_ = new ExtensionSpecialStoragePolicy(NULL);
    extensions::ManifestHandler::Register(keys::kIntents,
                                          new extensions::WebIntentsHandler);
  }

 protected:
  scoped_refptr<Extension> CreateProtectedApp() {
#if defined(OS_WIN)
    FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
    FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
    DictionaryValue manifest;
    manifest.SetString(keys::kName, "Protected");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kLaunchWebURL, "http://explicit/protected/start");
    ListValue* list = new ListValue();
    list->Append(Value::CreateStringValue("http://explicit/protected"));
    list->Append(Value::CreateStringValue("*://*.wildcards/protected"));
    manifest.Set(keys::kWebURLs, list);
    std::string error;
    scoped_refptr<Extension> protected_app = Extension::Create(
        path, Extension::INVALID, manifest, Extension::NO_FLAGS, &error);
    EXPECT_TRUE(protected_app.get()) << error;
    return protected_app;
  }

  scoped_refptr<Extension> CreateUnlimitedApp() {
#if defined(OS_WIN)
    FilePath path(FILE_PATH_LITERAL("c:\\bar"));
#elif defined(OS_POSIX)
    FilePath path(FILE_PATH_LITERAL("/bar"));
#endif
    DictionaryValue manifest;
    manifest.SetString(keys::kName, "Unlimited");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kLaunchWebURL, "http://explicit/unlimited/start");
    ListValue* list = new ListValue();
    list->Append(Value::CreateStringValue("unlimitedStorage"));
    manifest.Set(keys::kPermissions, list);
    list = new ListValue();
    list->Append(Value::CreateStringValue("http://explicit/unlimited"));
    list->Append(Value::CreateStringValue("*://*.wildcards/unlimited"));
    manifest.Set(keys::kWebURLs, list);
    std::string error;
    scoped_refptr<Extension> unlimited_app = Extension::Create(
        path, Extension::INVALID, manifest, Extension::NO_FLAGS, &error);
    EXPECT_TRUE(unlimited_app.get()) << error;
    return unlimited_app;
  }

  scoped_refptr<Extension> CreateComponentApp() {
#if defined(OS_WIN)
    FilePath path(FILE_PATH_LITERAL("c:\\component"));
#elif defined(OS_POSIX)
    FilePath path(FILE_PATH_LITERAL("/component"));
#endif
    DictionaryValue manifest;
    manifest.SetString(keys::kName, "Component");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kPublicKey,
        "MIGdMA0GCSqGSIb3DQEBAQUAA4GLADCBhwKBgQDOuXEIuoK1kAkBe0SKiJn/N9oNn3oU" \
        "xGa4dwj40MnJqPn+w0aR2vuyocm0R4Drp67aYwtLjOVPF4CICRq6ICP6eU07gGwQxGdZ" \
        "7HJASXV8hm0tab5I70oJmRLfFJyVAMCeWlFaOGq05v2i6EbifZM0qO5xALKNGQt+yjXi" \
        "5INM5wIBIw==");
    ListValue* list = new ListValue();
    list->Append(Value::CreateStringValue("unlimitedStorage"));
    list->Append(Value::CreateStringValue("fileSystem"));
    list->Append(Value::CreateStringValue("fileBrowserPrivate"));
    manifest.Set(keys::kPermissions, list);
    std::string error;
    scoped_refptr<Extension> component_app = Extension::Create(
        path, Extension::COMPONENT, manifest, Extension::NO_FLAGS, &error);
    EXPECT_TRUE(component_app.get()) << error;
    return component_app;
  }

  scoped_refptr<Extension> CreateHandlerApp() {
#if defined(OS_WIN)
    FilePath path(FILE_PATH_LITERAL("c:\\handler"));
#elif defined(OS_POSIX)
    FilePath path(FILE_PATH_LITERAL("/handler"));
#endif
    DictionaryValue manifest;
    manifest.SetString(keys::kName, "Handler");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kPublicKey,
        "MIGdMA0GCSqGSIb3DQEBAQUAA4GLADCBhwKBgQChptAQ0n4R56N03nWQ1ogR7DVRBjGo" \
        "80Vw6G9KLjzZv44D8rq5Q5IkeQrtKgWyZfXevlsCe3LaLo18rcz8iZx6lK2xhLdUR+OR" \
        "jsjuBfdEL5a5cWeRTSxf75AcqndQsmpwMBdrMTCZ8jQNusUI+XlrihLNNJuI5TM4vNIN" \
        "I5bYFQIBIw==");
    ListValue* list = new ListValue();
    list->Append(Value::CreateStringValue("unlimitedStorage"));
    list->Append(Value::CreateStringValue("fileSystem"));
    manifest.Set(keys::kPermissions, list);
    std::string error;
    scoped_refptr<Extension> handler_app = Extension::Create(
        path, Extension::INVALID, manifest, Extension::NO_FLAGS, &error);
    EXPECT_TRUE(handler_app.get()) << error;
    return handler_app;
  }

  scoped_refptr<Extension> CreateWebIntentViewApp() {
#if defined(OS_WIN)
    FilePath path(FILE_PATH_LITERAL("c:\\bar"));
#elif defined(OS_POSIX)
    FilePath path(FILE_PATH_LITERAL("/bar"));
#endif
    DictionaryValue manifest;
    manifest.SetString(keys::kName, "WebIntent");
    manifest.SetString(keys::kVersion, "1");
    manifest.SetString(keys::kLaunchWebURL, "http://explicit/unlimited/start");

    ListValue* view_intent_types = new ListValue;
    view_intent_types->Append(Value::CreateStringValue("text/plain"));

    DictionaryValue* view_intent = new DictionaryValue;
    view_intent->SetString(keys::kIntentTitle, "Test Intent");
    view_intent->Set(keys::kIntentType, view_intent_types);

    ListValue* view_intent_list = new ListValue;
    view_intent_list->Append(view_intent);

    DictionaryValue* intents = new DictionaryValue;
    intents->SetWithoutPathExpansion("http://webintents.org/view",
                                     view_intent_list);
    manifest.Set(keys::kIntents, intents);

    std::string error;
    scoped_refptr<Extension> intent_app = Extension::Create(
        path, Extension::INVALID, manifest, Extension::NO_FLAGS, &error);
    EXPECT_TRUE(intent_app.get()) << error;
    return intent_app;
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

  EXPECT_FALSE(policy_->IsStorageUnlimited(kHttpUrl));
  EXPECT_FALSE(policy_->IsStorageUnlimited(kHttpUrl));  // test cached result
  EXPECT_FALSE(policy_->IsStorageUnlimited(kExtensionUrl));
  ExtensionSet empty_set;
  ExpectProtectedBy(empty_set, kHttpUrl);

  // This one is just based on the scheme.
  EXPECT_TRUE(policy_->IsStorageProtected(kExtensionUrl));
}


TEST_F(ExtensionSpecialStoragePolicyTest, AppWithProtectedStorage) {
  scoped_refptr<Extension> extension(CreateProtectedApp());
  policy_->GrantRightsForExtension(extension);
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

  policy_->RevokeRightsForExtension(extension);
  ExpectProtectedBy(empty_set, GURL("http://explicit/"));
  ExpectProtectedBy(empty_set, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("https://bar.wildcards/"));
}

TEST_F(ExtensionSpecialStoragePolicyTest, AppWithUnlimitedStorage) {
  scoped_refptr<Extension> extension(CreateUnlimitedApp());
  policy_->GrantRightsForExtension(extension);
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

  policy_->RevokeRightsForExtension(extension);
  ExpectProtectedBy(empty_set, GURL("http://explicit/"));
  ExpectProtectedBy(empty_set, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("https://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("http://bar.wildcards/"));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://bar.wildcards/")));
}

TEST_F(ExtensionSpecialStoragePolicyTest, OverlappingApps) {
  scoped_refptr<Extension> protected_app(CreateProtectedApp());
  scoped_refptr<Extension> unlimited_app(CreateUnlimitedApp());
  policy_->GrantRightsForExtension(protected_app);
  policy_->GrantRightsForExtension(unlimited_app);
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

  policy_->RevokeRightsForExtension(unlimited_app);
  protecting_extensions.Remove(unlimited_app->id());
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy_->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  ExpectProtectedBy(protecting_extensions, GURL("http://explicit/"));
  ExpectProtectedBy(protecting_extensions, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(protecting_extensions, GURL("https://bar.wildcards/"));

  policy_->RevokeRightsForExtension(protected_app);
  ExpectProtectedBy(empty_set, GURL("http://explicit/"));
  ExpectProtectedBy(empty_set, GURL("http://foo.wildcards/"));
  ExpectProtectedBy(empty_set, GURL("https://bar.wildcards/"));
}

TEST_F(ExtensionSpecialStoragePolicyTest, WebIntentViewApp) {
  scoped_refptr<Extension> intent_app(CreateWebIntentViewApp());

  policy_->GrantRightsForExtension(intent_app);
  EXPECT_TRUE(policy_->IsFileHandler(intent_app->id()));

  policy_->RevokeRightsForExtension(intent_app);
  EXPECT_FALSE(policy_->IsFileHandler(intent_app->id()));
}

TEST_F(ExtensionSpecialStoragePolicyTest, HasSessionOnlyOrigins) {
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  TestingProfile profile;
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(&profile);
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
