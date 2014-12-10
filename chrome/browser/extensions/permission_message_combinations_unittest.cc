// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Contains;
using testing::Eq;

namespace extensions {

// Tests that ChromePermissionMessageProvider produces the expected messages for
// various combinations of app/extension permissions.
class PermissionMessageCombinationsUnittest : public testing::Test {
 public:
  PermissionMessageCombinationsUnittest()
      : message_provider_(new ChromePermissionMessageProvider()) {}
  ~PermissionMessageCombinationsUnittest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    // Force creation of ExtensionPrefs before adding extensions.
    env_.GetExtensionPrefs();
  }

 protected:
  // Create and install an app or extension with the given manifest JSON string.
  // Single-quotes in the string will be replaced with double quotes.
  void CreateAndInstall(const std::string& json_manifest) {
    std::string json_manifest_with_double_quotes = json_manifest;
    std::replace(json_manifest_with_double_quotes.begin(),
                 json_manifest_with_double_quotes.end(), '\'', '"');
    app_ = env_.MakeExtension(
        *base::test::ParseJson(json_manifest_with_double_quotes));
    // Add the app to any whitelists so we can test all permissions.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, app_->id());
  }

  // Checks whether the currently installed app or extension produces the given
  // permission messages. Call this after installing an app with the expected
  // permission messages. The messages are tested for existence in any order.
  testing::AssertionResult CheckManifestProducesPermissions() {
    return CheckManifestProducesPermissions(
        std::vector<std::string>(), GetPermissionMessages(), "messages");
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    return CheckManifestProducesPermissions(
        expected_messages, GetPermissionMessages(), "messages");
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    return CheckManifestProducesPermissions(
        expected_messages, GetPermissionMessages(), "messages");
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    return CheckManifestProducesPermissions(
        expected_messages, GetPermissionMessages(), "messages");
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3,
      const std::string& expected_message_4) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    return CheckManifestProducesPermissions(
        expected_messages, GetPermissionMessages(), "messages");
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3,
      const std::string& expected_message_4,
      const std::string& expected_message_5) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    expected_messages.push_back(expected_message_5);
    return CheckManifestProducesPermissions(
        expected_messages, GetPermissionMessages(), "messages");
  }

  // Checks whether the currently installed app or extension produces the given
  // host permission messages. Call this after installing an app with the
  // expected permission messages. The messages are tested for existence in any
  // order.
  testing::AssertionResult CheckManifestProducesHostPermissions() {
    return CheckManifestProducesPermissions(std::vector<std::string>(),
                                            GetHostPermissionMessages(),
                                            "host messages");
  }
  testing::AssertionResult CheckManifestProducesHostPermissions(
      const std::string& expected_message_1) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    return CheckManifestProducesPermissions(
        expected_messages, GetHostPermissionMessages(), "host messages");
  }
  testing::AssertionResult CheckManifestProducesHostPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    return CheckManifestProducesPermissions(
        expected_messages, GetHostPermissionMessages(), "host messages");
  }
  testing::AssertionResult CheckManifestProducesHostPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    return CheckManifestProducesPermissions(
        expected_messages, GetHostPermissionMessages(), "host messages");
  }
  testing::AssertionResult CheckManifestProducesHostPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3,
      const std::string& expected_message_4) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    return CheckManifestProducesPermissions(
        expected_messages, GetHostPermissionMessages(), "host messages");
  }
  testing::AssertionResult CheckManifestProducesHostPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3,
      const std::string& expected_message_4,
      const std::string& expected_message_5) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    expected_messages.push_back(expected_message_5);
    return CheckManifestProducesPermissions(
        expected_messages, GetHostPermissionMessages(), "host messages");
  }

 private:
  std::vector<base::string16> GetPermissionMessages() {
    return app_->permissions_data()->GetPermissionMessageStrings();
  }

  std::vector<base::string16> GetHostPermissionMessages() {
    std::vector<base::string16> details =
        app_->permissions_data()->GetPermissionMessageDetailsStrings();
    // If we have a host permission, exactly one message will contain the
    // details for it.
    for (const auto& host_string : details) {
      if (!host_string.empty()) {
        // The host_string will be a newline-separated string of entries.
        std::vector<base::string16> pieces;
        base::SplitString(host_string, base::char16('\n'), &pieces);
        return pieces;
      }
    }
    return std::vector<base::string16>();
  }

  testing::AssertionResult CheckManifestProducesPermissions(
      const std::vector<std::string>& expected_messages,
      const std::vector<base::string16>& actual_messages,
      const std::string& message_type_name) {
    if (expected_messages.size() != actual_messages.size()) {
      // Message: Expected 7 messages, got 5
      return testing::AssertionFailure()
             << "Expected " << expected_messages.size() << " "
             << message_type_name << ", got " << actual_messages.size() << ": "
             << MessagesVectorToString(actual_messages);
    }

    for (const auto& expected_message : expected_messages) {
      if (std::find(actual_messages.begin(), actual_messages.end(),
                    base::ASCIIToUTF16(expected_message)) ==
          actual_messages.end()) {
        // Message: Expected messages to contain "Foo", got { "Bar", "Baz" }
        return testing::AssertionFailure()
               << "Expected " << message_type_name << " to contain \""
               << expected_message << "\", got "
               << MessagesVectorToString(actual_messages);
      }
    }

    return testing::AssertionSuccess();
  }

  // Returns the vector of messages in a human-readable string format, e.g.:
  // { "Bar", "Baz" }
  base::string16 MessagesVectorToString(
      const std::vector<base::string16>& messages) {
    if (messages.empty())
      return base::ASCIIToUTF16("{}");
    return base::ASCIIToUTF16("{ \"") +
           JoinString(messages, base::ASCIIToUTF16("\", \"")) +
           base::ASCIIToUTF16("\" }");
  }

  extensions::TestExtensionEnvironment env_;
  scoped_ptr<ChromePermissionMessageProvider> message_provider_;
  scoped_refptr<const Extension> app_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMessageCombinationsUnittest);
};

// Test that the USB, Bluetooth and Serial permissions do not coalesce on their
// own, but do coalesce when more than 1 is present.
TEST_F(PermissionMessageCombinationsUnittest, USBSerialBluetoothCoalescing) {
  // Test that the USB permission does not coalesce on its own.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Test that the serial permission does not coalesce on its own.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'serial'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Access your serial devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Test that the bluetooth permission does not coalesce on its own.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'bluetooth': {}"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access information about Bluetooth devices paired with your system and "
      "discover nearby Bluetooth devices."));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Test that the bluetooth permission does not coalesce on its own, even
  // when it specifies additional permissions.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'bluetooth': {"
      "    'uuids': ['1105', '1106']"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access information about Bluetooth devices paired with your system and "
      "discover nearby Bluetooth devices."));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Test that the USB and Serial permissions coalesce.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] },"
      "    'serial'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor",
      "Access your serial devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Test that the USB, Serial and Bluetooth permissions coalesce.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] },"
      "    'serial',"
      "    'bluetooth'"
      "  ],"
      "  'bluetooth': {}"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor",
      "Access your Bluetooth and Serial devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Test that the USB, Serial and Bluetooth permissions coalesce even when
  // Bluetooth specifies multiple additional permissions.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] },"
      "    'serial',"
      "    'bluetooth'"
      "  ],"
      "  'bluetooth': {"
      "    'uuids': ['1105', '1106'],"
      "    'socket': true,"
      "    'low_energy': true"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor",
      "Access your Bluetooth and Serial devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// Test that the History permission takes precedence over the Tabs permission,
// and that the Sessions permission modifies this final message.
TEST_F(PermissionMessageCombinationsUnittest, TabsHistorySessionsCoalescing) {
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Read your browsing history"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'sessions'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'history'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your browsing history"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'history', 'sessions'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your browsing history on all your signed-in devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// Test that the fileSystem permission produces no messages by itself, unless it
// has both the 'write' and 'directory' additional permissions, in which case it
// displays a message.
TEST_F(PermissionMessageCombinationsUnittest, FileSystemReadWriteCoalescing) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'fileSystem'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'fileSystem', {'fileSystem': ['retainEntries', 'write']}"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'fileSystem', {'fileSystem': ["
      "      'retainEntries', 'write', 'directory'"
      "    ]}"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Write to files and folders that you open in the application"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// Check that host permission messages are generated correctly when URLs are
// entered as permissions.
TEST_F(PermissionMessageCombinationsUnittest, HostsPermissionMessages) {
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on www.blogger.com"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites and "
      "www.blogger.com"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites, all news.com sites, "
      "and www.blogger.com"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on a number of websites"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions(
      "All google.com sites", "All news.com sites", "www.blogger.com",
      "www.foobar.com"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on a number of websites"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions(
      "All go.com sites", "All google.com sites", "All news.com sites",
      "www.blogger.com", "www.foobar.com"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://*.go.com/',"
      "    'chrome://favicon/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all go.com sites",
      "Read the icons of the websites you visit"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Having the 'all sites' permission doesn't change the permission message,
  // since its pseudo-granted at runtime.
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://*.go.com/',"
      "    'chrome://favicon/',"
      "    'http://*.*',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all go.com sites",
      "Read the icons of the websites you visit"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// Test that hosted apps are not given any messages for host permissions.
TEST_F(PermissionMessageCombinationsUnittest,
       PackagedAppsHaveNoHostPermissions) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'serial',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Access your serial devices"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// Test various apps with lots of permissions, including those with no
// permission messages, or those that only apply to apps or extensions even when
// the given manifest is for a different type.
TEST_F(PermissionMessageCombinationsUnittest, PermissionMessageCombos) {
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs',"
      "    'bookmarks',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'unlimitedStorage',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites and www.blogger.com",
      "Read your browsing history", "Read and change your bookmarks"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs',"
      "    'sessions',"
      "    'bookmarks',"
      "    'unlimitedStorage',"
      "    'syncFileSystem',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices",
      "Read and change your bookmarks",
      "Read and change your data on a number of websites"));
  ASSERT_TRUE(CheckManifestProducesHostPermissions(
      "All go.com sites", "All google.com sites", "All news.com sites",
      "www.blogger.com", "www.foobar.com"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs',"
      "    'sessions',"
      "    'bookmarks',"
      "    'accessibilityFeatures.read',"
      "    'accessibilityFeatures.modify',"
      "    'alarms',"
      "    'browsingData',"
      "    'cookies',"
      "    'desktopCapture',"
      "    'gcm',"
      "    'topSites',"
      "    'storage',"
      "    'unlimitedStorage',"
      "    'syncFileSystem',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");

#ifdef OS_CHROMEOS
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices",
      "Capture content of your screen", "Read and change your bookmarks",
      "Read and change your data on a number of websites",
      "Read and change your accessibility settings"));
#else
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices",
      "Capture content of your screen", "Read and change your bookmarks",
      "Read and change your data on a number of websites"));
#endif

  ASSERT_TRUE(CheckManifestProducesHostPermissions(
      "All go.com sites", "All google.com sites", "All news.com sites",
      "www.blogger.com", "www.foobar.com"));

  // Create an App instead, ensuring that the host permission messages are not
  // added.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'contextMenus',"
      "    'bluetooth',"
      "    'permissions',"
      "    'accessibilityFeatures.read',"
      "    'accessibilityFeatures.modify',"
      "    'alarms',"
      "    'power',"
      "    'cookies',"
      "    'serial',"
      "    'usb',"
      "    'storage',"
      "    'gcm',"
      "    'topSites',"
      "    'storage',"
      "    'unlimitedStorage',"
      "    'syncFileSystem',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");

#ifdef OS_CHROMEOS
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access your serial devices", "Store data in your Google Drive account",
      "Read and change your accessibility settings"));
#else
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access your serial devices", "Store data in your Google Drive account"));
#endif

  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// Tests that the 'plugin' manifest key produces the correct permission.
TEST_F(PermissionMessageCombinationsUnittest, PluginPermission) {
  // Extensions can have plugins.
  CreateAndInstall(
      "{"
      "  'plugins': ["
      "    { 'path': 'extension_plugin.dll' }"
      "  ]"
      "}");

#ifdef OS_CHROMEOS
  ASSERT_TRUE(CheckManifestProducesPermissions());
#else
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change all your data on your computer and the websites you "
      "visit"));
#endif

  ASSERT_TRUE(CheckManifestProducesHostPermissions());

  // Apps can't have plugins.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'plugins': ["
      "    { 'path': 'extension_plugin.dll' }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());
  ASSERT_TRUE(CheckManifestProducesHostPermissions());
}

// TODO(sashab): Add a test that checks that messages are generated correctly
// for withheld permissions, when an app is granted the 'all sites' permission.

// TODO(sashab): Add a test that ensures that all permissions that can generate
// a coalesced message can also generate a message on their own (i.e. ensure
// that no permissions only modify other permissions).

// TODO(sashab): Add a test for every permission message combination that can
// generate a message.

// TODO(aboxhall): Add tests for the automation API permission messages.

}  // namespace extensions
