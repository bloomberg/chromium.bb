// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include "ui/base/test/ui_controls.h"

#if defined(USE_AURA)
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/base/test/ui_controls_aura.h"
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/test/ui_controls_factory_desktop_aurax11.h"
#endif
#endif

#if defined(OS_CHROMEOS)
#include "ash/test/ui_controls_factory_ash.h"
#endif

int main(int argc, char** argv) {
  // Only allow ui_controls to be used in interactive_ui_tests, since they
  // depend on focus and can't be sharded.
  ui_controls::EnableUIControls();

#if defined(OS_CHROMEOS)
  ui_controls::InstallUIControlsAura(ash::test::CreateAshUIControls());
#elif defined(USE_AURA)

#if defined(OS_LINUX)
  ui_controls::InstallUIControlsAura(
      views::test::CreateUIControlsDesktopAura());
#else
  // TODO(win_ash): when running interactive_ui_tests for Win Ash, use above.
  ui_controls::InstallUIControlsAura(aura::test::CreateUIControlsAura(NULL));
#endif
#endif

  // Run interactive_ui_tests serially, they do not support running in parallel.
  return LaunchChromeTests(1, argc, argv);
}
