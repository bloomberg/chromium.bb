// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

// Tests that ChromePermissionMessageProvider provides correct permission
// messages for given permissions.
// NOTE: No extensions are created as part of these tests. Integration tests
// that test the messages are generated properly for extensions can be found in
// chrome/browser/extensions/permission_messages_unittest.cc.
class ChromePermissionMessageProviderUnittest : public testing::Test {
 public:
  ChromePermissionMessageProviderUnittest()
      : message_provider_(new ChromePermissionMessageProvider()) {}
  ~ChromePermissionMessageProviderUnittest() override {}

 protected:
  std::vector<base::string16> GetMessages(APIPermissionSet& permissions,
                                          Manifest::Type type) {
    scoped_refptr<const PermissionSet> permission_set = new PermissionSet(
        permissions, ManifestPermissionSet(), URLPatternSet(), URLPatternSet());
    return message_provider_->GetWarningMessages(permission_set.get(), type);
  }

 private:
  scoped_ptr<ChromePermissionMessageProvider> message_provider_;

  DISALLOW_COPY_AND_ASSIGN(ChromePermissionMessageProviderUnittest);
};

// Checks that if an app has a superset and a subset permission, only the
// superset permission message is displayed if thye are both present.
TEST_F(ChromePermissionMessageProviderUnittest,
       SupersetOverridesSubsetPermission) {
  APIPermissionSet permissions;
  std::vector<base::string16> messages;

  permissions.clear();
  permissions.insert(APIPermission::kTab);
  messages = GetMessages(permissions, Manifest::TYPE_PLATFORM_APP);
  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      messages[0]);

  permissions.clear();
  permissions.insert(APIPermission::kTopSites);
  messages = GetMessages(permissions, Manifest::TYPE_PLATFORM_APP);
  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_TOPSITES),
            messages[0]);

  permissions.clear();
  permissions.insert(APIPermission::kTab);
  permissions.insert(APIPermission::kTopSites);
  messages = GetMessages(permissions, Manifest::TYPE_PLATFORM_APP);
  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      messages[0]);
}

}  // namespace extensions
