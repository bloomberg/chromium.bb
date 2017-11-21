// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/vr_test_suite.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace vr {

VrTestSuite::VrTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

VrTestSuite::~VrTestSuite() = default;

void VrTestSuite::Initialize() {
  base::TestSuite::Initialize();

  scoped_task_environment_ =
      base::MakeUnique<base::test::ScopedTaskEnvironment>(
          base::test::ScopedTaskEnvironment::MainThreadType::UI);

  mojo::edk::Init();

  base::FilePath pak_path;
#if defined(OS_ANDROID)
  ui::RegisterPathProvider();
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
  PathService::Get(base::DIR_MODULE, &pak_path);
#endif
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_test.pak"));
}

void VrTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace vr
