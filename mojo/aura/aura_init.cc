// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/aura_init.h"

#include "mojo/aura/screen_mojo.h"
#include "ui/aura/env.h"

namespace mojo {

AuraInit::AuraInit() {
  aura::Env::CreateInstance(false);

  screen_.reset(ScreenMojo::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
}

AuraInit::~AuraInit() {
}

}  // namespace mojo
