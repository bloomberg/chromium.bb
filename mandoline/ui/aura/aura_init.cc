// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/aura_init.h"

#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "components/view_manager/public/cpp/view.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace mandoline {

namespace {

// Paths resources are loaded from.
const char kResourceUIPak[] = "mandoline_ui.pak";

std::set<std::string> GetResourcePaths() {
  std::set<std::string> paths;
  paths.insert(kResourceUIPak);
  return paths;
}

}  // namespace

// TODO(sky): the 1.f should be view->viewport_metrics().device_scale_factor,
// but that causes clipping problems. No doubt we're not scaling a size
// correctly.
AuraInit::AuraInit(mojo::View* view, mojo::Shell* shell)
    : ui_init_(view->viewport_metrics().size_in_pixels.To<gfx::Size>(), 1.f) {
  aura::Env::CreateInstance(false);

  InitializeResources(shell);

  ui::InitializeInputMethodForTesting();
}

AuraInit::~AuraInit() {
}

void AuraInit::InitializeResources(mojo::Shell* shell) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;
  resource_provider::ResourceLoader resource_loader(shell, GetResourcePaths());
  if (!resource_loader.BlockUntilLoaded())
    return;
  CHECK(resource_loader.loaded());
  base::i18n::InitializeICUWithFileDescriptor(
      resource_loader.GetICUFile().TakePlatformFile(),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::RegisterPathProvider();
  ui::ResourceBundle::InitSharedInstanceWithPakPath(base::FilePath());
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      resource_loader.ReleaseFile(kResourceUIPak),
      ui::SCALE_FACTOR_100P);
  // There is a bunch of static state in gfx::Font, by running this now,
  // before any other apps load, we ensure all the state is set up.
  gfx::Font();
}

}  // namespace mandoline
