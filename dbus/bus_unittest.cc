// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used to test AddFilterFunction().
DBusHandlerResult DummyHandler(DBusConnection* connection,
                               DBusMessage* raw_message,
                               void* user_data) {
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

}  // namespace

TEST(BusTest, GetObjectProxy) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  dbus::ObjectProxy* object_proxy1 =
      bus->GetObjectProxy("org.chromium.TestService",
                          dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  dbus::ObjectProxy* object_proxy2 =
      bus->GetObjectProxy("org.chromium.TestService",
                          dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  dbus::ObjectProxy* object_proxy3 =
      bus->GetObjectProxy(
          "org.chromium.TestService",
          dbus::ObjectPath("/org/chromium/DifferentTestObject"));
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);

  bus->ShutdownAndBlock();
}

TEST(BusTest, GetObjectProxyIgnoreUnknownService) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  dbus::ObjectProxy* object_proxy1 =
      bus->GetObjectProxyWithOptions(
          "org.chromium.TestService",
          dbus::ObjectPath("/org/chromium/TestObject"),
          dbus::ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  dbus::ObjectProxy* object_proxy2 =
      bus->GetObjectProxyWithOptions(
          "org.chromium.TestService",
          dbus::ObjectPath("/org/chromium/TestObject"),
          dbus::ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  dbus::ObjectProxy* object_proxy3 =
      bus->GetObjectProxyWithOptions(
          "org.chromium.TestService",
          dbus::ObjectPath("/org/chromium/DifferentTestObject"),
          dbus::ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);

  bus->ShutdownAndBlock();
}

TEST(BusTest, RemoveObjectProxy) {
  // Setup the current thread's MessageLoop.
  MessageLoop message_loop;

  // Start the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_IO;
  base::Thread dbus_thread("D-Bus thread");
  dbus_thread.StartWithOptions(thread_options);

  // Create the bus.
  dbus::Bus::Options options;
  options.dbus_task_runner = dbus_thread.message_loop_proxy();
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  // Try to remove a non existant object proxy should return false.
  ASSERT_FALSE(
      bus->RemoveObjectProxy("org.chromium.TestService",
                             dbus::ObjectPath("/org/chromium/TestObject"),
                             base::Bind(&base::DoNothing)));

  dbus::ObjectProxy* object_proxy1 =
      bus->GetObjectProxy("org.chromium.TestService",
                          dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // Increment the reference count to the object proxy to avoid destroying it
  // while removing the object.
  object_proxy1->AddRef();

  // Remove the object from the bus. This will invalidate any other usage of
  // object_proxy1 other than destroy it. We keep this object for a comparison
  // at a later time.
  ASSERT_TRUE(
      bus->RemoveObjectProxy("org.chromium.TestService",
                             dbus::ObjectPath("/org/chromium/TestObject"),
                             base::Bind(&base::DoNothing)));

  // This should return a different object because the first object was removed
  // from the bus, but not deleted from memory.
  dbus::ObjectProxy* object_proxy2 =
      bus->GetObjectProxy("org.chromium.TestService",
                          dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);

  // Compare the new object with the first object. The first object still exists
  // thanks to the increased reference.
  EXPECT_NE(object_proxy1, object_proxy2);

  // Release object_proxy1.
  object_proxy1->Release();

  // Shut down synchronously.
  bus->ShutdownOnDBusThreadAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
  dbus_thread.Stop();
}

TEST(BusTest, GetExportedObject) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  dbus::ExportedObject* object_proxy1 =
      bus->GetExportedObject(dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  dbus::ExportedObject* object_proxy2 =
      bus->GetExportedObject(dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  dbus::ExportedObject* object_proxy3 =
      bus->GetExportedObject(
          dbus::ObjectPath("/org/chromium/DifferentTestObject"));
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);

  bus->ShutdownAndBlock();
}

TEST(BusTest, UnregisterExportedObject) {
  // Start the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_IO;
  base::Thread dbus_thread("D-Bus thread");
  dbus_thread.StartWithOptions(thread_options);

  // Create the bus.
  dbus::Bus::Options options;
  options.dbus_task_runner = dbus_thread.message_loop_proxy();
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  dbus::ExportedObject* object_proxy1 =
      bus->GetExportedObject(dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // Increment the reference count to the object proxy to avoid destroying it
  // calling UnregisterExportedObject. This ensures the dbus::ExportedObject is
  // not freed from memory. See http://crbug.com/137846 for details.
  object_proxy1->AddRef();

  bus->UnregisterExportedObject(dbus::ObjectPath("/org/chromium/TestObject"));

  // This should return a new object because the object_proxy1 is still in
  // alloc'ed memory.
  dbus::ExportedObject* object_proxy2 =
      bus->GetExportedObject(dbus::ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);
  EXPECT_NE(object_proxy1, object_proxy2);

  // Release the incremented reference.
  object_proxy1->Release();

  // Shut down synchronously.
  bus->ShutdownOnDBusThreadAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
  dbus_thread.Stop();
}

TEST(BusTest, ShutdownAndBlock) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  // Shut down synchronously.
  bus->ShutdownAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
}

TEST(BusTest, ShutdownAndBlockWithDBusThread) {
  // Start the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_IO;
  base::Thread dbus_thread("D-Bus thread");
  dbus_thread.StartWithOptions(thread_options);

  // Create the bus.
  dbus::Bus::Options options;
  options.dbus_task_runner = dbus_thread.message_loop_proxy();
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  // Shut down synchronously.
  bus->ShutdownOnDBusThreadAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
  dbus_thread.Stop();
}

TEST(BusTest, AddFilterFunction) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  // Should connect before calling AddFilterFunction().
  bus->Connect();

  int data1 = 100;
  int data2 = 200;
  ASSERT_TRUE(bus->AddFilterFunction(&DummyHandler, &data1));
  // Cannot add the same function with the same data.
  ASSERT_FALSE(bus->AddFilterFunction(&DummyHandler, &data1));
  // Can add the same function with different data.
  ASSERT_TRUE(bus->AddFilterFunction(&DummyHandler, &data2));

  ASSERT_TRUE(bus->RemoveFilterFunction(&DummyHandler, &data1));
  ASSERT_FALSE(bus->RemoveFilterFunction(&DummyHandler, &data1));
  ASSERT_TRUE(bus->RemoveFilterFunction(&DummyHandler, &data2));

  bus->ShutdownAndBlock();
}

TEST(BusTest, DoubleAddAndRemoveMatch) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  dbus::ScopedDBusError error;

  bus->Connect();

  // Adds the same rule twice.
  bus->AddMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get());
  ASSERT_FALSE(error.is_set());

  bus->AddMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get());
  ASSERT_FALSE(error.is_set());

  // Removes the same rule twice.
  ASSERT_TRUE(bus->RemoveMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get()));
  ASSERT_FALSE(error.is_set());

  // The rule should be still in the bus since it was removed only once.
  // A second removal shouldn't give an error.
  ASSERT_TRUE(bus->RemoveMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get()));
  ASSERT_FALSE(error.is_set());

  // A third attemp to remove the same rule should fail.
  ASSERT_FALSE(bus->RemoveMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get()));

  bus->ShutdownAndBlock();
}
