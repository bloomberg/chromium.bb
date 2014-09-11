// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_client_bundle.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(DBusClientBundleTest, UnstubFlagParser) {
  EXPECT_EQ(0, DBusClientBundle::ParseUnstubList("foo"));

  EXPECT_EQ(DBusClientBundle::BLUETOOTH,
            DBusClientBundle::ParseUnstubList("BLUETOOTH"));

  EXPECT_EQ(DBusClientBundle::BLUETOOTH,
            DBusClientBundle::ParseUnstubList("bluetooth"));

  EXPECT_EQ(
      DBusClientBundle::CRAS |
          DBusClientBundle::CROS_DISKS |
          DBusClientBundle::DEBUG_DAEMON |
          DBusClientBundle::SHILL,
      DBusClientBundle::ParseUnstubList(
            "Cras,Cros_Disks,debug_daemon,Shill"));

  EXPECT_EQ(
      DBusClientBundle::CRAS |
          DBusClientBundle::CROS_DISKS |
          DBusClientBundle::DEBUG_DAEMON |
          DBusClientBundle::SHILL,
      DBusClientBundle::ParseUnstubList(
            "foo,Cras,Cros_Disks,debug_daemon,Shill"));
}

}  // namespace chromeos
