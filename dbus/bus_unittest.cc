// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include "base/memory/ref_counted.h"
#include "dbus/exported_object.h"
#include "dbus/object_proxy.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(BusTest, GetObjectProxy) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  dbus::ObjectProxy* object_proxy1 =
      bus->GetObjectProxy("org.chromium.TestService",
                          "/org/chromium/TestObject");
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  dbus::ObjectProxy* object_proxy2 =
      bus->GetObjectProxy("org.chromium.TestService",
                          "/org/chromium/TestObject");
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  dbus::ObjectProxy* object_proxy3 =
      bus->GetObjectProxy("org.chromium.TestService",
                          "/org/chromium/DifferentTestObject");
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);
}

TEST(BusTest, GetExportedObject) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  dbus::ExportedObject* object_proxy1 =
      bus->GetExportedObject("org.chromium.TestService",
                             "/org/chromium/TestObject");
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  dbus::ExportedObject* object_proxy2 =
      bus->GetExportedObject("org.chromium.TestService",
                             "/org/chromium/TestObject");
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  dbus::ExportedObject* object_proxy3 =
      bus->GetExportedObject("org.chromium.TestService",
                             "/org/chromium/DifferentTestObject");
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);
}
