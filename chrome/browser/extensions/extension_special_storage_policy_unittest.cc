// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

class ExtensionSpecialStoragePolicyTest : public testing::Test {
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
        path, Extension::INVALID, manifest, false, &error);
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
        path, Extension::INVALID, manifest, false, &error);
    EXPECT_TRUE(unlimited_app.get()) << error;
    return unlimited_app;
  }
};

TEST_F(ExtensionSpecialStoragePolicyTest, EmptyPolicy) {
  const GURL kHttpUrl("http://foo");
  const GURL kExtensionUrl("chrome-extension://bar");

  scoped_refptr<ExtensionSpecialStoragePolicy> policy(
      new ExtensionSpecialStoragePolicy);

  ASSERT_FALSE(policy->IsStorageUnlimited(kHttpUrl));
  ASSERT_FALSE(policy->IsStorageUnlimited(kHttpUrl));  // test cached result
  ASSERT_FALSE(policy->IsStorageUnlimited(kExtensionUrl));
  ASSERT_FALSE(policy->IsStorageProtected(kHttpUrl));

  // This one is just based on the scheme.
  ASSERT_TRUE(policy->IsStorageProtected(kExtensionUrl));
}


TEST_F(ExtensionSpecialStoragePolicyTest, AppWithProtectedStorage) {
  scoped_refptr<Extension> extension(CreateProtectedApp());
  scoped_refptr<ExtensionSpecialStoragePolicy> policy(
      new ExtensionSpecialStoragePolicy);
  policy->GrantRightsForExtension(extension);
  EXPECT_FALSE(policy->IsStorageUnlimited(extension->url()));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("https://bar.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://not_listed/")));

  policy->RevokeRightsForExtension(extension);
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://foo.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("https://bar.wildcards/")));
}

TEST_F(ExtensionSpecialStoragePolicyTest, AppWithUnlimitedStorage) {
  scoped_refptr<Extension> extension(CreateUnlimitedApp());
  scoped_refptr<ExtensionSpecialStoragePolicy> policy(
      new ExtensionSpecialStoragePolicy);
  policy->GrantRightsForExtension(extension);
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://bar.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://not_listed/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(extension->url()));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("http://not_listed/")));

  policy->RevokeRightsForExtension(extension);
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://bar.wildcards/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("https://bar.wildcards/")));
}

TEST_F(ExtensionSpecialStoragePolicyTest, OverlappingApps) {
  scoped_refptr<Extension> protected_app(CreateProtectedApp());
  scoped_refptr<Extension> unlimited_app(CreateUnlimitedApp());
  scoped_refptr<ExtensionSpecialStoragePolicy> policy(
      new ExtensionSpecialStoragePolicy);
  policy->GrantRightsForExtension(protected_app);
  policy->GrantRightsForExtension(unlimited_app);

  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://bar.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://not_listed/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("http://explicit:6000/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("http://not_listed/")));

  policy->RevokeRightsForExtension(unlimited_app);
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("http://explicit/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("https://foo.wildcards/")));
  EXPECT_FALSE(policy->IsStorageUnlimited(GURL("https://bar.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("http://foo.wildcards/")));
  EXPECT_TRUE(policy->IsStorageProtected(GURL("https://bar.wildcards/")));

  policy->RevokeRightsForExtension(protected_app);
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://explicit/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("http://foo.wildcards/")));
  EXPECT_FALSE(policy->IsStorageProtected(GURL("https://bar.wildcards/")));
}
