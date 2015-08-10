// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_interactive_ui_test_base.h"

#include "base/path_service.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ash {
namespace test {

AshInteractiveUITestBase::AshInteractiveUITestBase() {}

AshInteractiveUITestBase::~AshInteractiveUITestBase() {}

void AshInteractiveUITestBase::SetUp() {
  gfx::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  PathService::Get(base::DIR_MODULE, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);
  aura::Env::CreateInstance(true);

  test::AshTestBase::SetUp();
}

void AshInteractiveUITestBase::TearDown() {
  test::AshTestBase::TearDown();
  aura::Env::DeleteInstance();
}

}  // namespace test
}  // namespace ash
