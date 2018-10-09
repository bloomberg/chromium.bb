// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main entry point for all unit tests.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_module.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

credential_provider::CGaiaCredentialProviderModule _AtlModule;

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleMock(&argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
