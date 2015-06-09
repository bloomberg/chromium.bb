// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/aura_init.h"

#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "mandoline/ui/aura/screen_mojo.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_ANDROID)
#include "components/resource_provider/public/cpp/resource_loader.h"
#endif

namespace mandoline {

#if defined(OS_ANDROID)
namespace {

// Paths resources are loaded from.
const char kResourceIcudtl[] = "icudtl.dat";
const char kResourceUIPak[] = "ui_test.pak";

std::set<std::string> GetResourcePaths() {
  std::set<std::string> paths;
  paths.insert(kResourceIcudtl);
  paths.insert(kResourceUIPak);
  return paths;
}

}  // namespace
#endif  // defined(OS_ANDROID)

AuraInit::AuraInit(mojo::Shell* shell) {
  aura::Env::CreateInstance(false);

  screen_.reset(ScreenMojo::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  InitializeResources(shell);
}

AuraInit::~AuraInit() {
}

void AuraInit::InitializeResources(mojo::Shell* shell) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;
#if defined(OS_ANDROID)
  resource_provider::ResourceLoader resource_loader(shell, GetResourcePaths());
  if (!resource_loader.BlockUntilLoaded())
    return;
  CHECK(resource_loader.loaded());
  base::i18n::InitializeICUWithFileDescriptor(
      resource_loader.ReleaseFile(kResourceIcudtl).TakePlatformFile(),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::RegisterPathProvider();
  ui::ResourceBundle::InitSharedInstanceWithPakPath(base::FilePath());
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      resource_loader.ReleaseFile(kResourceUIPak),
      ui::SCALE_FACTOR_100P);
#else
  base::i18n::InitializeICU();

  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
#endif

  // There is a bunch of static state in gfx::Font, by running this now,
  // before any other apps load, we ensure all the state is set up.
  gfx::Font();
}

}  // namespace mandoline
