// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_suite.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/base/win/atl_module.h"
#endif

namespace ash {
namespace test {

AuraShellTestSuite::AuraShellTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {
}

AuraShellTestSuite::~AuraShellTestSuite() {
}

void AuraShellTestSuite::Initialize() {
  base::TestSuite::Initialize();
  gfx::GLSurfaceTestSupport::InitializeOneOff();

#if defined(OS_WIN)
  base::win::Version version = base::win::GetVersion();
  // Although Ash officially is only supported for users on Win7+, we still run
  // ash_unittests on Vista builders, so we still need to initialize COM.
  if (version >= base::win::VERSION_VISTA &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop)) {
    com_initializer_.reset(new base::win::ScopedCOMInitializer());
    ui::win::CreateATLModuleIfNeeded();
  }
#endif

  gfx::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Force unittests to run using en-US so if we test against string output,
  // it'll pass regardless of the system language.
  base::i18n::SetICUDefaultLocale("en_US");

  // Load ash resources and en-US strings; not 'common' (Chrome) resources.
  // TODO(msw): Check ResourceBundle::IsScaleFactorSupported; load 300% etc.
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  base::FilePath ash_test_strings =
      path.Append(FILE_PATH_LITERAL("ash_test_strings.pak"));
  base::FilePath ash_test_resources_100 =
      path.Append(FILE_PATH_LITERAL("ash_test_resources_100_percent.pak"));
  base::FilePath ash_test_resources_200 =
      path.Append(FILE_PATH_LITERAL("ash_test_resources_200_percent.pak"));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(ash_test_strings);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  rb.AddDataPackFromPath(ash_test_resources_100, ui::SCALE_FACTOR_100P);
  rb.AddDataPackFromPath(ash_test_resources_200, ui::SCALE_FACTOR_200P);

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  aura::Env::CreateInstance(true);
}

void AuraShellTestSuite::Shutdown() {
  aura::Env::DeleteInstance();
  ui::ResourceBundle::CleanupSharedInstance();
#if defined(OS_WIN)
  com_initializer_.reset();
#endif
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace ash
