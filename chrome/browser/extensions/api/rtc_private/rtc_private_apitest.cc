// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/extensions/api/rtc_private/rtc_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/rtc_private/events.
const char kTestRtcExtensionId[] = "jdaiaafaoeaejklkkbndacnggmgmpkpa";
// Test contact data.
const char kContactFullName[] = "Test Contact";
const char kTestEmail_1[] = "test_1@something.com";
const char kTestEmail_2[] = "test_2@something.com";
const char kTestPhone_1[] = "(555) 111-2222";
const char kTestPhone_2[] = "(555) 333-4444";

}  // namespace

class RtcPrivateApiTest : public ExtensionApiTest {
 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kWhitelistedExtensionID,
                                    kTestRtcExtensionId);
    command_line->AppendSwitch(switches::kEnableContacts);
  }

  static contacts::Contact* CreateTestContact() {
    contacts::Contact* contact = new contacts::Contact();
    contact->set_full_name(kContactFullName);
    contacts::Contact_EmailAddress* address_1 =
        contact->mutable_email_addresses()->Add();
    address_1->set_address(kTestEmail_1);
    contacts::Contact_EmailAddress* address_2 =
        contact->mutable_email_addresses()->Add();
    address_2->set_address(kTestEmail_2);

    contacts::Contact_PhoneNumber* number_1 =
        contact->mutable_phone_numbers()->Add();
    number_1->set_number(kTestPhone_1);
    contacts::Contact_PhoneNumber* number_2 =
        contact->mutable_phone_numbers()->Add();
    number_2->set_number(kTestPhone_2);
    return contact;
  }
};

IN_PROC_BROWSER_TEST_F(RtcPrivateApiTest, LaunchEvents) {
  // Load test RTC extension.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("rtc_private/events"));
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener discovery_started("received_all", false);

  // Raise all RTC-related events.
  scoped_ptr<contacts::Contact> contact(CreateTestContact());
  extensions::RtcPrivateEventRouter::DispatchLaunchEvent(
      browser()->profile(),
      extensions::RtcPrivateEventRouter::LAUNCH_ACTIVATE,
      contact.get());

  extensions::RtcPrivateEventRouter::DispatchLaunchEvent(
      browser()->profile(),
      extensions::RtcPrivateEventRouter::LAUNCH_CHAT,
      contact.get());

  extensions::RtcPrivateEventRouter::DispatchLaunchEvent(
      browser()->profile(),
      extensions::RtcPrivateEventRouter::LAUNCH_VIDEO,
      contact.get());

  extensions::RtcPrivateEventRouter::DispatchLaunchEvent(
      browser()->profile(),
      extensions::RtcPrivateEventRouter::LAUNCH_VOICE,
      contact.get());

  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());
}
