// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/view_event_test_platform_part.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

namespace {

// ViewEventTestPlatformPart implementation for Views, but non-CrOS.
class ViewEventTestPlatformPartDefault : public ViewEventTestPlatformPart {
 public:
  explicit ViewEventTestPlatformPartDefault(
      ui::ContextFactory* context_factory) {
#if defined(USE_AURA)
    screen_.reset(views::CreateDesktopScreen());
    gfx::Screen::SetScreenInstance(screen_.get());
    aura::Env::CreateInstance(true);
    aura::Env::GetInstance()->set_context_factory(context_factory);
#endif
  }

  ~ViewEventTestPlatformPartDefault() override {
#if defined(USE_AURA)
    aura::Env::DeleteInstance();
    gfx::Screen::SetScreenInstance(nullptr);
#endif
  }

  // Overridden from ViewEventTestPlatformPart:
  gfx::NativeWindow GetContext() override { return NULL; }

 private:
  scoped_ptr<gfx::Screen> screen_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestPlatformPartDefault);
};

}  // namespace

// static
ViewEventTestPlatformPart* ViewEventTestPlatformPart::Create(
    ui::ContextFactory* context_factory) {
  return new ViewEventTestPlatformPartDefault(context_factory);
}
