// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/chrome_browser_main_extra_parts_aura.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/aura/active_desktop_monitor.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/aura/env.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget_aura.h"

#if defined(USE_X11) && !defined(OS_CHROMEOS)
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "chrome/common/pref_names.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/linux_ui/linux_ui.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_init.h"
#if defined(OS_WIN)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif  // defined(OS_WIN)
#endif  // defined(USE_ASH)

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

namespace {

#if defined(USE_X11) && !defined(OS_CHROMEOS)
ui::NativeTheme* GetNativeThemeForWindow(aura::Window* window) {
  if (!window)
    return NULL;

  Profile* profile = NULL;
  if (window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
      window->type() == ui::wm::WINDOW_TYPE_POPUP) {
    profile = reinterpret_cast<Profile*>(
        window->GetNativeWindowProperty(Profile::kProfileKey));
  }

  if (profile && !profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme))
    return ui::NativeThemeAura::instance();

  return NULL;
}
#endif

#if !defined(OS_CHROMEOS) && defined(USE_ASH)
// Returns the desktop this process was initially launched in.
chrome::HostDesktopType GetInitialDesktop() {
#if defined(OS_WIN) && defined(USE_ASH)
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kViewerConnect) ||
      command_line->HasSwitch(switches::kViewerLaunchViaAppId)) {
    return chrome::HOST_DESKTOP_TYPE_ASH;
  }
#elif defined(OS_LINUX)
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOpenAsh))
    return chrome::HOST_DESKTOP_TYPE_ASH;
#endif

  return chrome::HOST_DESKTOP_TYPE_NATIVE;
}
#endif  // !defined(OS_CHROMEOS) && defined(USE_ASH)

}  // namespace

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura() {
}

ChromeBrowserMainExtraPartsAura::~ChromeBrowserMainExtraPartsAura() {
}

void ChromeBrowserMainExtraPartsAura::PreEarlyInitialization() {
#if defined(USE_X11) && !defined(OS_CHROMEOS)
  if (GetInitialDesktop() != chrome::HOST_DESKTOP_TYPE_ASH) {
    // TODO(erg): Refactor this into a dlopen call when we add a GTK3 port.
    views::LinuxUI* gtk2_ui = BuildGtk2UI();
    gtk2_ui->SetNativeThemeOverride(base::Bind(&GetNativeThemeForWindow));
    views::LinuxUI::SetInstance(gtk2_ui);
  } else {
    // TODO(erg): Eventually, we'll need to somehow support IMEs in ash on
    // Linux.
    ui::InitializeInputMethodForTesting();
  }
#endif
}

void ChromeBrowserMainExtraPartsAura::ToolkitInitialized() {
#if !defined(OS_CHROMEOS)
#if defined(USE_ASH)
  CHECK(aura::Env::GetInstance());
  active_desktop_monitor_.reset(new ActiveDesktopMonitor(GetInitialDesktop()));
#endif
#endif

#if defined(USE_X11) && !defined(OS_CHROMEOS)
  if (GetInitialDesktop() != chrome::HOST_DESKTOP_TYPE_ASH)
    views::LinuxUI::instance()->Initialize();
#endif
}

void ChromeBrowserMainExtraPartsAura::PreCreateThreads() {
#if !defined(OS_CHROMEOS)
#if defined(USE_ASH)
  if (!chrome::ShouldOpenAshOnStartup())
#endif
  {
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   views::CreateDesktopScreen());
  }
#endif
}

void ChromeBrowserMainExtraPartsAura::PreProfileInit() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Now that we have some minimal ui initialized, check to see if we're
  // running as root and bail if we are.
  DetectRunningAsRoot();
#endif
}

void ChromeBrowserMainExtraPartsAura::PostMainMessageLoopRun() {
  active_desktop_monitor_.reset();

  // aura::Env instance is deleted in BrowserProcessImpl::StartTearDown
  // after the metrics service is deleted.
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void ChromeBrowserMainExtraPartsAura::DetectRunningAsRoot() {
  if (getuid() == 0) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kUserDataDir))
      return;

    base::string16 title = l10n_util::GetStringFUTF16(
        IDS_REFUSE_TO_RUN_AS_ROOT,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    base::string16 message = l10n_util::GetStringFUTF16(
        IDS_REFUSE_TO_RUN_AS_ROOT_2,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

    chrome::ShowMessageBox(NULL,
                           title,
                           message,
                           chrome::MESSAGE_BOX_TYPE_WARNING);

    // Avoids gpu_process_transport_factory.cc(153)] Check failed:
    // per_compositor_data_.empty() when quit is chosen.
    base::RunLoop().RunUntilIdle();

    exit(EXIT_FAILURE);
  }
}
#endif
