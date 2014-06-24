// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/views/views_init_internal.h"

#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace mojo {

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

}  // namespace mojo
