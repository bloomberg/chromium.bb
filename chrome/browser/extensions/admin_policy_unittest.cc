// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/admin_policy.h"

#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using extensions::Extension;

namespace ap = extensions::admin_policy;

class ExtensionAdminPolicyTest : public testing::Test {
 public:
  void CreateExtension(Extension::Location location, bool required) {
    ASSERT_EQ(required, Extension::IsRequired(location));

    DictionaryValue values;
    values.SetString(extension_manifest_keys::kName, "test");
    values.SetString(extension_manifest_keys::kVersion, "0.1");
    std::string error;
    extension_ = Extension::Create(FilePath(), location, values,
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
  blacklist.Append(Value::CreateStringValue("http://www.google.com"));
  EXPECT_FALSE(ap::BlacklistedByDefault(&blacklist));
  blacklist.Append(Value::CreateStringValue("*"));
  EXPECT_TRUE(ap::BlacklistedByDefault(&blacklist));

  blacklist.Clear();
  blacklist.Append(Value::CreateStringValue("*"));
  EXPECT_TRUE(ap::BlacklistedByDefault(&blacklist));
}

// Tests UserMayLoad for required extensions.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadRequired) {
  CreateExtension(Extension::EXTERNAL_POLICY_DOWNLOAD, true);
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, extension_.get(), NULL));
  string16 error;
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  // Required extensions may load even if they're on the blacklist.
  base::ListValue blacklist;
  blacklist.Append(Value::CreateStringValue(extension_->id()));
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), NULL));

  blacklist.Append(Value::CreateStringValue("*"));
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), NULL));
}

// Tests UserMayLoad when no blacklist exists, or it's empty.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadNoBlacklist) {
  CreateExtension(Extension::INTERNAL, false);
  EXPECT_TRUE(ap::UserMayLoad(NULL, NULL, extension_.get(), NULL));
  base::ListValue blacklist;
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), NULL));
  string16 error;
  EXPECT_TRUE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the whitelist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadWhitelisted) {
  CreateExtension(Extension::INTERNAL, false);

  base::ListValue whitelist;
  whitelist.Append(Value::CreateStringValue(extension_->id()));
  EXPECT_TRUE(ap::UserMayLoad(NULL, &whitelist, extension_.get(), NULL));

  base::ListValue blacklist;
  blacklist.Append(Value::CreateStringValue(extension_->id()));
  EXPECT_TRUE(ap::UserMayLoad(NULL, &whitelist, extension_.get(), NULL));
  string16 error;
  EXPECT_TRUE(ap::UserMayLoad(NULL, &whitelist, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the blacklist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadBlacklisted) {
  CreateExtension(Extension::INTERNAL, false);

  // Blacklisted by default.
  base::ListValue blacklist;
  blacklist.Append(Value::CreateStringValue("*"));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), NULL));
  string16 error;
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  // Extension on the blacklist, with and without wildcard.
  blacklist.Append(Value::CreateStringValue(extension_->id()));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), NULL));
  blacklist.Clear();
  blacklist.Append(Value::CreateStringValue(extension_->id()));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, NULL, extension_.get(), NULL));

  // With a whitelist. There's no such thing as a whitelist wildcard.
  base::ListValue whitelist;
  whitelist.Append(
      Value::CreateStringValue("behllobkkfkfnphdnhnkndlbkcpglgmj"));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, &whitelist, extension_.get(), NULL));
  whitelist.Append(Value::CreateStringValue("*"));
  EXPECT_FALSE(ap::UserMayLoad(&blacklist, &whitelist, extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayModifySettings) {
  CreateExtension(Extension::INTERNAL, false);
  EXPECT_TRUE(ap::UserMayModifySettings(extension_.get(), NULL));
  string16 error;
  EXPECT_TRUE(ap::UserMayModifySettings(extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  CreateExtension(Extension::EXTERNAL_POLICY_DOWNLOAD, true);
  error.clear();
  EXPECT_FALSE(ap::UserMayModifySettings(extension_.get(), NULL));
  EXPECT_FALSE(ap::UserMayModifySettings(extension_.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(ExtensionAdminPolicyTest, MustRemainEnabled) {
  CreateExtension(Extension::EXTERNAL_POLICY_DOWNLOAD, true);
  EXPECT_TRUE(ap::MustRemainEnabled(extension_.get(), NULL));
  string16 error;
  EXPECT_TRUE(ap::MustRemainEnabled(extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  CreateExtension(Extension::INTERNAL, false);
  error.clear();
  EXPECT_FALSE(ap::MustRemainEnabled(extension_.get(), NULL));
  EXPECT_FALSE(ap::MustRemainEnabled(extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}
