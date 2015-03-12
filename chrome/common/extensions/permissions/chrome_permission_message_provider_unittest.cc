// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/url_pattern_set.h"
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
  std::vector<base::string16> GetMessages(const APIPermissionSet& permissions,
                                          Manifest::Type type) {
    scoped_refptr<const PermissionSet> permission_set = new PermissionSet(
        permissions, ManifestPermissionSet(), URLPatternSet(), URLPatternSet());
    return message_provider_->GetWarningMessages(permission_set.get(), type);
  }

  std::vector<base::string16> GetDetails(const APIPermissionSet& permissions,
                                         Manifest::Type type) {
    scoped_refptr<const PermissionSet> permission_set = new PermissionSet(
        permissions, ManifestPermissionSet(), URLPatternSet(), URLPatternSet());
    return message_provider_->GetWarningMessagesDetails(permission_set.get(),
                                                        type);
  }

 private:
  scoped_ptr<ChromePermissionMessageProvider> message_provider_;

  DISALLOW_COPY_AND_ASSIGN(ChromePermissionMessageProviderUnittest);
};

// Checks that if an app has a superset and a subset permission, only the
// superset permission message is displayed if they are both present.
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

// Checks that when permissions are merged into a single message, their details
// are merged as well.
TEST_F(ChromePermissionMessageProviderUnittest,
       WarningsAndDetailsCoalesceTogether) {
  // kTab and kTopSites should be merged into a single message.
  APIPermissionSet permissions;
  permissions.insert(APIPermission::kTab);
  permissions.insert(APIPermission::kTopSites);
  // The USB device permission message has a non-empty details string.
  scoped_ptr<UsbDevicePermission> usb(new UsbDevicePermission(
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice)));
  scoped_ptr<base::ListValue> devices_list(new base::ListValue());
  devices_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138c, -1).ToValue().release());
  devices_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138d, -1).ToValue().release());
  ASSERT_TRUE(usb->FromValue(devices_list.get(), nullptr, nullptr));
  permissions.insert(usb.release());

  std::vector<base::string16> messages =
      GetMessages(permissions, Manifest::TYPE_EXTENSION);
  std::vector<base::string16> details =
      GetDetails(permissions, Manifest::TYPE_EXTENSION);

  ASSERT_EQ(2U, messages.size());
  ASSERT_EQ(messages.size(), details.size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      messages[0]);
  EXPECT_TRUE(details[0].empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
      messages[1]);
  EXPECT_FALSE(details[1].empty());
}

}  // namespace extensions
