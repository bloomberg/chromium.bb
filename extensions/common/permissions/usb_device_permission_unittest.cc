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

#if defined(ENABLE_EXTENSIONS)
#define MAYBE_PermissionMessage PermissionMessage
#else
#define MAYBE_PermissionMessage DISABLED_PermissionMessage
#endif  // defined(ENABLE_EXTENSIONS)

TEST(USBDevicePermissionTest, MAYBE_PermissionMessage) {
  const char* const kMessages[] = {
      "Access the USB device PVR Mass Storage from HUMAX Co., Ltd.",
      "Access the USB device from HUMAX Co., Ltd.",
      "Access the USB device",
  };

  // Prepare data set
  scoped_ptr<base::ListValue> permission_list(new base::ListValue());
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138c, -1).ToValue()->DeepCopy());
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138d, -1).ToValue()->DeepCopy());
  permission_list->Append(
      UsbDevicePermissionData(0x02ae, 0x138d, -1).ToValue()->DeepCopy());

  UsbDevicePermission permission(
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
  ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

  PermissionMessages messages = permission.GetMessages();
  ASSERT_EQ(3U, messages.size());
  EXPECT_EQ(base::ASCIIToUTF16(kMessages[0]), messages.at(0).message());
  EXPECT_EQ(base::ASCIIToUTF16(kMessages[1]), messages.at(1).message());
  EXPECT_EQ(base::ASCIIToUTF16(kMessages[2]), messages.at(2).message());
}

}  // namespace extensions
