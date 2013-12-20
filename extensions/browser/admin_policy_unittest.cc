// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/admin_policy.h"

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using extensions::Extension;
using extensions::Manifest;

namespace ap = extensions::admin_policy;

class ExtensionAdminPolicyTest : public testing::Test {
 public:
  void CreateExtension(Manifest::Location location) {
    base::DictionaryValue values;
    CreateExtensionFromValues(location, &values);
  }

  void CreateHostedApp(Manifest::Location location) {
    base::DictionaryValue values;
    values.Set(extensions::manifest_keys::kWebURLs, new base::ListValue());
    values.SetString(extensions::manifest_keys::kLaunchWebURL,
                     "http://www.example.com");
    CreateExtensionFromValues(location, &values);
  }

  void CreateExtensionFromValues(Manifest::Location location,
                                 base::DictionaryValue* values) {
    values->SetString(extensions::manifest_keys::kName, "test");
    values->SetString(extensions::manifest_keys::kVersion, "0.1");
    std::string error;
    extension_ = Extension::Create(base::FilePath(), location, *values,
                                   Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension_.get());
  }

 protected:
  scoped_refptr<Extension> extension_;
};

// Tests the flag value indicating that extensions are blacklisted by default.
TEST_F(ExtensionAdminPolicyTest, BlacklistedByDefault) {
  EXPECT_FALSE(ap::BlacklistedByDefault(NULL));

  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("http://www.google.com"));
  EXPECT_FALSE(ap::BlacklistedByDefault(&blacklist));
  blacklist.Append(new base::StringValue("*"));
  EXPECT_TRUE(ap::BlacklistedByDefault(&blacklist));

  blacklist.Clear();
  blacklist.Append(new base::StringValue("*"));
  EXPECT_TRUE(ap::BlacklistedByDefault(&blacklist));
}

// Tests UserMayLoad for required extensions.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadRequired) {
  CreateExtension(Manifest::COMPONENT);
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(),
                              &error));
  EXPECT_TRUE(error.empty());

  // Required extensions may load even if they're on the blacklist.
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                              NULL));

  blacklist.Append(new base::StringValue("*"));
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                              NULL));
}

// Tests UserMayLoad when no blacklist exists, or it's empty.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadNoBlacklist) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));
  base::ListValue blacklist;
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                              NULL));
  base::string16 error;
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                              &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the whitelist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadWhitelisted) {
  CreateExtension(Manifest::INTERNAL);

  base::ListValue whitelist;
  whitelist.Append(new base::StringValue(extension_->id()));
  EXPECT_TRUE(ap::UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(),
                              NULL));

  base::ListValue blacklist;
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_TRUE(ap::UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(),
                              NULL));
  base::string16 error;
  EXPECT_TRUE(ap::UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(),
                              &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the blacklist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadBlacklisted) {
  CreateExtension(Manifest::INTERNAL);

  // Blacklisted by default.
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("*"));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                               NULL));
  base::string16 error;
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                               &error));
  EXPECT_FALSE(error.empty());

  // Extension on the blacklist, with and without wildcard.
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                               NULL));
  blacklist.Clear();
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(),
                               NULL));

  // With a whitelist. There's no such thing as a whitelist wildcard.
  base::ListValue whitelist;
  whitelist.Append(
      new base::StringValue("behllobkkfkfnphdnhnkndlbkcpglgmj"));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, &whitelist, NULL, NULL,
                               extension_.get(), NULL));
  whitelist.Append(new base::StringValue("*"));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, &whitelist, NULL, NULL,
                               extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayLoadAllowedTypes) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));

  base::ListValue allowed_types;
  EXPECT_FALSE(ap::UserMayLoad(NULL, NULL, NULL, &allowed_types,
                               extension_.get(), NULL));

  allowed_types.AppendInteger(Manifest::TYPE_EXTENSION);
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, NULL, &allowed_types,
                              extension_.get(), NULL));

  CreateHostedApp(Manifest::INTERNAL);
  EXPECT_FALSE(ap::UserMayLoad(NULL, NULL, NULL, &allowed_types,
                               extension_.get(), NULL));

  CreateHostedApp(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_FALSE(ap::UserMayLoad(NULL, NULL, NULL, &allowed_types,
                               extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayModifySettings) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(ap::UserMayModifySettings(extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(ap::UserMayModifySettings(extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  error.clear();
  EXPECT_FALSE(ap::UserMayModifySettings(extension_.get(), NULL));
  EXPECT_FALSE(ap::UserMayModifySettings(extension_.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(ExtensionAdminPolicyTest, MustRemainEnabled) {
  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_TRUE(ap::MustRemainEnabled(extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(ap::MustRemainEnabled(extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  CreateExtension(Manifest::INTERNAL);
  error.clear();
  EXPECT_FALSE(ap::MustRemainEnabled(extension_.get(), NULL));
  EXPECT_FALSE(ap::MustRemainEnabled(extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}
