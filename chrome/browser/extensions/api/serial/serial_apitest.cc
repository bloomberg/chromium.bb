// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

class SerialApiTest : public PlatformAppApiTest {
 public:
  SerialApiTest() {}
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialExtension) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("serial_port", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("serial/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

#if 0
  // Enable this path only if all the following are true:
  //
  // 1. You have an Arduino or compatible board attached to your machine and
  // properly appearing as the first virtual serial port ("first" is very
  // loosely defined as whichever port shows up in serial.getPorts). We've
  // tested only the Atmega32u4 Breakout Board and Arduino Leonardo; note that
  // both these boards are based on the Atmel ATmega32u4, rather than the more
  // common Arduino '328p with either FTDI or '8/16u2 USB interfaces. TODO:
  // test more widely.
  //
  // 2. Your user has permission to read/write the port. For example, this
  // might mean that your user is in the "tty" or "uucp" group on Ubuntu
  // flavors of Linux, or else that the port's path (e.g., /dev/ttyACM0) has
  // global read/write permissions.
  //
  // 3. You have uploaded a program to the board that does a byte-for-byte echo
  // on the virtual serial port at 57600 bps. An example is at
  // chrome/test/data/extensions/api_test/serial/api/serial_arduino_test.ino.
  //
  listener.Reply("echo_device_attached");
#else
  listener.Reply("false");
#endif

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
