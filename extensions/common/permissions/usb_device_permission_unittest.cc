// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(USBDevicePermissionTest, PermissionDataOrder) {
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, -1),
            UsbDevicePermissionData(0x02ad, 0x138d, -1));
  ASSERT_LT(UsbDevicePermissionData(0x02ad, 0x138d, -1),
            UsbDevicePermissionData(0x02ae, 0x138c, -1));
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, -1),
            UsbDevicePermissionData(0x02ad, 0x138c, 0));
}

TEST(USBDevicePermissionTest, SingleDevicePermissionMessages) {
  const char* const kMessages[] = {
      "Access any PVR Mass Storage from HUMAX Co., Ltd. via USB",
      "Access USB devices from HUMAX Co., Ltd.",
      "Access USB devices from an unknown vendor",
  };

  {
    scoped_ptr<base::ListValue> permission_list(new base::ListValue());
    permission_list->Append(
        UsbDevicePermissionData(0x02ad, 0x138c, -1).ToValue().release());

    UsbDevicePermission permission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
    ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

    PermissionMessages messages = permission.GetMessages();
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(base::ASCIIToUTF16(kMessages[0]), messages.at(0).message());
  }
  {
    scoped_ptr<base::ListValue> permission_list(new base::ListValue());
    permission_list->Append(
        UsbDevicePermissionData(0x02ad, 0x138d, -1).ToValue().release());

    UsbDevicePermission permission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
    ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

    PermissionMessages messages = permission.GetMessages();
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(base::ASCIIToUTF16(kMessages[1]), messages.at(0).message());
  }
  {
    scoped_ptr<base::ListValue> permission_list(new base::ListValue());
    permission_list->Append(
        UsbDevicePermissionData(0x02ae, 0x138d, -1).ToValue().release());

    UsbDevicePermission permission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
    ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

    PermissionMessages messages = permission.GetMessages();
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(base::ASCIIToUTF16(kMessages[2]), messages.at(0).message());
  }
}

TEST(USBDevicePermissionTest, MultipleDevicePermissionMessage) {
  const char* const kMessage = "Access any of these USB devices";
  const char* const kDetails =
      "PVR Mass Storage from HUMAX Co., Ltd.\n"
      "unknown devices from HUMAX Co., Ltd.\n"
      "devices from an unknown vendor";

  // Prepare data set
  scoped_ptr<base::ListValue> permission_list(new base::ListValue());
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138c, -1).ToValue().release());
  // This device's product ID is not in Chrome's database.
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138d, -1).ToValue().release());
  // This additional unknown product will be collapsed into the entry above.
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138e, -1).ToValue().release());
  // This device's vendor ID is not in Chrome's database.
  permission_list->Append(
      UsbDevicePermissionData(0x02ae, 0x138d, -1).ToValue().release());
  // This additional unknown vendor will be collapsed into the entry above.
  permission_list->Append(
      UsbDevicePermissionData(0x02af, 0x138d, -1).ToValue().release());

  UsbDevicePermission permission(
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
  ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

  PermissionMessages messages = permission.GetMessages();
  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(base::ASCIIToUTF16(kMessage), messages.at(0).message());
  EXPECT_EQ(base::ASCIIToUTF16(kDetails), messages.at(0).details());
}

}  // namespace extensions
