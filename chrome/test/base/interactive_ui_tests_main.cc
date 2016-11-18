// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/base/test/ui_controls.h"

#if defined(USE_AURA)
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/base/test/ui_controls_aura.h"
#if defined(USE_X11) && !defined(OS_CHROMEOS)
#include "ui/views/test/ui_controls_factory_desktop_aurax11.h"
#endif
#endif

#if defined(OS_CHROMEOS)
#include "ash/test/ui_controls_factory_ash.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "chrome/test/base/always_on_top_window_killer_win.h"
#endif

class InteractiveUITestSuite : public ChromeTestSuite {
 public:
  InteractiveUITestSuite(int argc, char** argv) : ChromeTestSuite(argc, argv) {}
  ~InteractiveUITestSuite() override {}

 protected:
  // ChromeTestSuite overrides:
  void Initialize() override {
    ChromeTestSuite::Initialize();

#if defined(OS_MACOSX)
    gpu::ImageTransportSurface::SetAllowOSMesaForTesting(true);
#endif

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
#if defined(USE_OZONE)
    NOTIMPLEMENTED();
#else
    ui_controls::InstallUIControlsAura(
        views::test::CreateUIControlsDesktopAura());
#endif  // defined(USE_OZONE)
#else
    // TODO(win_ash): when running interactive_ui_tests for Win Ash, use above.
    ui_controls::InstallUIControlsAura(aura::test::CreateUIControlsAura(NULL));
#endif  // defined(OS_LINUX)
#endif  // defined(USE_AURA)
  }

  void Shutdown() override {
#if defined(OS_WIN)
    com_initializer_.reset();
#endif
  }

 private:
#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif
};

class InteractiveUITestSuiteRunner : public ChromeTestSuiteRunner {
 public:
  int RunTestSuite(int argc, char** argv) override {
    return InteractiveUITestSuite(argc, argv).Run();
  }
};

int main(int argc, char** argv) {
#if defined(OS_WIN)
  KillAlwaysOnTopWindows(RunType::BEFORE_TEST);
#endif
  // TODO(sky): this causes a crash in an autofill test on macosx, figure out
  // why: http://crbug.com/641969.
#if !defined(OS_MACOSX)
  // Without this it's possible for the first browser to start up in the
  // background, generally because the last test did something that causes the
  // test to run in the background. Most interactive ui tests assume they are in
  // the foreground and fail in weird ways if they aren't (for example, clicks
  // go to the wrong place). This ensures the first browser is always in the
  // foreground.
  InProcessBrowserTest::set_global_browser_set_up_function(
      &ui_test_utils::BringBrowserWindowToFront);
#endif
  // Run interactive_ui_tests serially, they do not support running in parallel.
  int default_jobs = 1;
  InteractiveUITestSuiteRunner runner;
  ChromeTestLauncherDelegate delegate(&runner);
  const int result = LaunchChromeTests(default_jobs, &delegate, argc, argv);
#if defined(OS_WIN)
  KillAlwaysOnTopWindows(RunType::AFTER_TEST);
#endif
  return result;
}
