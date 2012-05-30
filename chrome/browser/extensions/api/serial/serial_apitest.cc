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
  // 1. You have an Adafruit Atmega32u4 Breakout Board attached to your machine
  // and properly appearing as the first virtual serial port ("first" is very
  // loosely defined as whichever port shows up in serial.getPorts). Other
  // devices will work, but this is the only one tested.
  //
  // 2. Your user has permission to read/write the port. For example, this might
  // mean that your user is in the "tty" group on Ubuntu flavors of Linux, or
  // else that /dev/ttyACM0 has global read/write permissions.
  //
  // 4. You have uploaded a program to the board that does a byte-for-byte echo
  // on the virtual serial port at 57600 bps. Here is an example (built using
  // the Arduino IDE):
  //
  // void setup() {
  //   Serial.begin(57600);
  // }
  //
  // void loop() {
  //   while (true) {
  //     while (Serial.available() > 0) {
  //       Serial.print((char)Serial.read());
  //     }
  //   }
  // }
  listener.Reply("echo_device_attached");
#else
  listener.Reply("false");
#endif

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
