// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

TEST_F(ExtensionManifestTest, ExperimentalPermission) {
  LoadAndExpectError("experimental.json", errors::kExperimentalFlagRequired);
  LoadAndExpectSuccess("experimental.json", extensions::Manifest::COMPONENT);
  LoadAndExpectSuccess("experimental.json", extensions::Manifest::INTERNAL,
                       extensions::Extension::FROM_WEBSTORE);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);
  LoadAndExpectSuccess("experimental.json");
}
