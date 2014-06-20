// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionManifestServiceWorkerTest : public ExtensionManifestTest {
 public:
  ExtensionManifestServiceWorkerTest()
      : trunk_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

  void AddServiceWorkerCommandLineSwitch() {
    CHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExperimentalWebPlatformFeatures));
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  // "app.service_worker" is restricted to trunk in _manifest_features.json.
  extensions::ScopedCurrentChannel trunk_channel_;
};

// Checks that a service_worker key is ignored without
// enable-experimental-web-platform-features switch. When service workers are
// enabled by default please remove this test.
TEST_F(ExtensionManifestServiceWorkerTest, ServiceWorkerCommandLineRequired) {
  CHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalWebPlatformFeatures));
  LoadFromStringAndExpectError(
      "{"
      "  'name': '',"
      "  'manifest_version': 2,"
      "  'version': '1',"
      "  'app': {"
      "    'service_worker': {"
      "      'script': 'service_worker.js'"
      "    }"
      "  }"
      "}",
      manifest_errors::kServiceWorkerRequiresFlag);
}

// Checks that an app manifest with a service_worker key but no script fails.
TEST_F(ExtensionManifestServiceWorkerTest, ServiceWorkerEmpty) {
  AddServiceWorkerCommandLineSwitch();
  LoadFromStringAndExpectError(
      "{"
      "  'name': '',"
      "  'manifest_version': 2,"
      "  'version': '1',"
      "  'app': {"
      "    'service_worker': {}"  // No script specified.
      "  }"
      "}",
      manifest_errors::kBackgroundRequiredForPlatformApps);

  LoadFromStringAndExpectError(
      "{"
      "  'name': '',"
      "  'manifest_version': 2,"
      "  'version': '1',"
      "  'app': {"
      "    'service_worker': {"
      "      'script': ''"  // Empty script.
      "    }"
      "  }"
      "}",
      manifest_errors::kBackgroundRequiredForPlatformApps);
}

// Checks that an app manifest with a single script is loaded.
TEST_F(ExtensionManifestServiceWorkerTest, ServiceWorkerScript) {
  AddServiceWorkerCommandLineSwitch();
  scoped_refptr<Extension> extension(LoadFromStringAndExpectSuccess(
      "{"
      "  'name': '',"
      "  'manifest_version': 2,"
      "  'version': '1',"
      "  'app': {"
      "    'service_worker': {"
      "      'script': 'service_worker.js'"
      "    }"
      "  }"
      "}"));
  ASSERT_TRUE(extension.get());
  // "app.service_worker" key exists and access is permitted.
  EXPECT_TRUE(extension->manifest()->HasPath("app.service_worker"));
  EXPECT_EQ("service_worker.js",
            BackgroundInfo::GetServiceWorkerScript(extension.get()));

  EXPECT_TRUE(BackgroundInfo::HasServiceWorker(extension.get()));
}

// Checks that an app manifest with service worker and background script fails.
TEST_F(ExtensionManifestServiceWorkerTest, ServiceWorkerWithBackgroundScript) {
  AddServiceWorkerCommandLineSwitch();
  LoadFromStringAndExpectError(
      "{"
      "  'name': '',"
      "  'manifest_version': 2,"
      "  'version': '1',"
      "  'app': {"
      "    'service_worker': {"
      "      'script': 'service_worker.js'"
      "    },"
      "    'background': {"
      "      'scripts': [ 'background.js' ]"
      "    }"
      "  }"
      "}",
      manifest_errors::kInvalidBackgroundCombination);
}

}  // namespace extensions
