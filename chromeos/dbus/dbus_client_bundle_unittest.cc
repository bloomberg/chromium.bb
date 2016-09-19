// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_client_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Makes mask operations more readable.
DBusClientTypeMask AsClientTypeMask(DBusClientType type) {
  return static_cast<DBusClientTypeMask>(type);
}

}  // namespace

TEST(DBusClientBundleTest, RealClientsFlagParser) {
  EXPECT_EQ(AsClientTypeMask(DBusClientType::NONE),
            ParseDBusRealClientsList("foo"));

  EXPECT_EQ(AsClientTypeMask(DBusClientType::BLUETOOTH),
            ParseDBusRealClientsList("BLUETOOTH"));

  EXPECT_EQ(AsClientTypeMask(DBusClientType::BLUETOOTH),
            ParseDBusRealClientsList("bluetooth"));

  EXPECT_EQ(AsClientTypeMask(DBusClientType::CRAS) |
                AsClientTypeMask(DBusClientType::CROS_DISKS) |
                AsClientTypeMask(DBusClientType::DEBUG_DAEMON) |
                AsClientTypeMask(DBusClientType::SHILL),
            ParseDBusRealClientsList("Cras,Cros_Disks,debug_daemon,Shill"));

  EXPECT_EQ(AsClientTypeMask(DBusClientType::CRAS) |
                AsClientTypeMask(DBusClientType::CROS_DISKS) |
                AsClientTypeMask(DBusClientType::DEBUG_DAEMON) |
                AsClientTypeMask(DBusClientType::SHILL),
            ParseDBusRealClientsList("foo,Cras,Cros_Disks,debug_daemon,Shill"));
}

}  // namespace chromeos
