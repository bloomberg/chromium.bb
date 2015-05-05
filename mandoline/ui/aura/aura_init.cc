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

namespace mandoline {
namespace {

// Used to ensure we only init once.
class Setup {
 public:
  Setup() {
    base::i18n::InitializeICU();

    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    // There is a bunch of static state in gfx::Font, by running this now,
    // before any other apps load, we ensure all the state is set up.
    gfx::Font();
  }

  ~Setup() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitViews() {
  setup.Get();
}

AuraInit::AuraInit() {
  aura::Env::CreateInstance(false);

  screen_.reset(ScreenMojo::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());

  InitViews();
}

AuraInit::~AuraInit() {
}

}  // namespace mandoline
