// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_suite.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor_setup.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/gfx/gfx_paths.h"

#if defined(OS_MACOSX)
#include "ash/test/test_suite_init.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/base/win/atl_module.h"
#include "win8/test/metro_registration_helper.h"
#include "win8/test/open_with_dialog_controller.h"
#include "win8/test/test_registrar_constants.h"
#endif

namespace ash {
namespace test {

AuraShellTestSuite::AuraShellTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

void AuraShellTestSuite::Initialize() {
  base::TestSuite::Initialize();

#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop)) {
    ASSERT_TRUE(win8::RegisterTestDefaultBrowser(
        win8::test::kDefaultTestAppUserModelId,
        win8::test::kDefaultTestExeName));

    ui::win::CreateATLModuleIfNeeded();

    std::vector<string16> choices;
    win8::OpenWithDialogController controller;
    controller.RunSynchronously(NULL, L"http", win8::test::kDefaultTestExeName,
                                &choices);
  }
#endif

  gfx::RegisterPathProvider();
  ui::RegisterPathProvider();

#if defined(OS_MACOSX)
  ash::test::OverrideFrameworkBundle();
#endif

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  ui::CompositorTestSupport::Initialize();
  ui::SetupTestCompositor();
}

void AuraShellTestSuite::Shutdown() {
  ui::CompositorTestSupport::Terminate();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace ash
