// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/switches.h"

using extensions::Extension;
using extensions::FeatureSwitch;

class ExtensionOptionsApiTest : public ExtensionApiTest {
 private:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Need to add a command line flag as well as a FeatureSwitch because the
    // FeatureSwitch is not copied over to the renderer process from the
    // browser process.
    command_line->AppendSwitch(
        extensions::switches::kEnableEmbeddedExtensionOptions);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionOptionsApiTest, ExtensionCanEmbedOwnOptions) {
  FeatureSwitch::ScopedOverride enable_options(
      FeatureSwitch::embedded_extension_options(), true);

  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("extension_options").AppendASCII("embed_self"),
      1);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(RunExtensionSubtest("extension_options/embed_self", "test.html"));
}
