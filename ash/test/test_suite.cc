// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_suite.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
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
#endif

namespace ash {
namespace test {

AuraShellTestSuite::AuraShellTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

void AuraShellTestSuite::Initialize() {
  base::TestSuite::Initialize();

  gfx::RegisterPathProvider();
  ui::RegisterPathProvider();

#if defined(OS_MACOSX)
  ash::test::OverrideFrameworkBundle();
#endif

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  ui::CompositorTestSupport::Initialize();

#if defined(OS_WIN)
  // The glue code that connects the tests to the metro viewer process depends
  // on using the real compositor.
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    ui::SetupTestCompositor();
#else
  ui::SetupTestCompositor();
#endif
}

void AuraShellTestSuite::Shutdown() {
  ui::CompositorTestSupport::Terminate();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace ash
