// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include "chrome/test/base/chrome_test_suite.h"
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

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

class InteractiveUITestSuite : public ChromeTestSuite {
 public:
  InteractiveUITestSuite(int argc, char** argv) : ChromeTestSuite(argc, argv) {}
  virtual ~InteractiveUITestSuite() {}

 protected:
  // ChromeTestSuite overrides:
  virtual void Initialize() OVERRIDE {
    ChromeTestSuite::Initialize();

    // Only allow ui_controls to be used in interactive_ui_tests, since they
    // depend on focus and can't be sharded.
    ui_controls::EnableUIControls();

#if defined(OS_CHROMEOS)
    ui_controls::InstallUIControlsAura(ash::test::CreateAshUIControls());
#elif defined(USE_AURA)
#if defined(OS_WIN)
    com_initializer_.reset(new base::win::ScopedCOMInitializer());
#endif

#if defined(OS_LINUX)
    ui_controls::InstallUIControlsAura(
        views::test::CreateUIControlsDesktopAura());
#else
    // TODO(win_ash): when running interactive_ui_tests for Win Ash, use above.
    ui_controls::InstallUIControlsAura(aura::test::CreateUIControlsAura(NULL));
#endif
#endif
  }

  virtual void Shutdown() OVERRIDE {
#if defined(OS_WIN)
    com_initializer_.reset();
#endif
  }

 private:
#if defined(OS_WIN)
  scoped_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif
};

class InteractiveUITestSuiteRunner : public ChromeTestSuiteRunner {
 public:
  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return InteractiveUITestSuite(argc, argv).Run();
  }
};

int main(int argc, char** argv) {
  // Run interactive_ui_tests serially, they do not support running in parallel.
  int default_jobs = 1;
  InteractiveUITestSuiteRunner runner;
  return LaunchChromeTests(default_jobs, &runner, argc, argv);
}
