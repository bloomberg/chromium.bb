// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_dark_aura.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {

ui::NativeTheme* GetNativeThemeForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  Profile* profile = nullptr;
  // Window types not listed here (such as tooltips) will never use Chrome
  // theming.
  if (window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
      window->type() == ui::wm::WINDOW_TYPE_POPUP) {
    profile = reinterpret_cast<Profile*>(
        window->GetNativeWindowProperty(Profile::kProfileKey));
  }

  // If using the system (GTK) theme, don't use an Aura NativeTheme at all.
  // NB: ThemeService::UsingSystemTheme() might lag behind this pref. See
  // http://crbug.com/585522
  if (!profile || profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme))
    return nullptr;

  // Use a dark theme for incognito browser windows that aren't
  // custom-themed. Otherwise, normal Aura theme.
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE &&
      ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme() &&
      BrowserView::GetBrowserViewForNativeWindow(window)) {
    return ui::NativeThemeDarkAura::instance();
  }

  return ui::NativeThemeAura::instance();
}

}  // namespace

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() {
}

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() {}

void ChromeBrowserMainExtraPartsViewsLinux::PreEarlyInitialization() {
  // TODO(erg): Refactor this into a dlopen call when we add a GTK3 port.
  views::LinuxUI* gtk2_ui = BuildGtk2UI();
  gtk2_ui->SetNativeThemeOverride(base::Bind(&GetNativeThemeForWindow));
  views::LinuxUI::SetInstance(gtk2_ui);
}

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();
  views::LinuxUI::instance()->Initialize();
}

void ChromeBrowserMainExtraPartsViewsLinux::PreCreateThreads() {
  ChromeBrowserMainExtraPartsViews::PreCreateThreads();
  // TODO(varkha): The next call should not be necessary once Material Design is
  // on unconditionally.
  views::LinuxUI::instance()->MaterialDesignControllerReady();
  views::LinuxUI::instance()->UpdateDeviceScaleFactor(
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
}

void ChromeBrowserMainExtraPartsViewsLinux::PreProfileInit() {
  // On the Linux desktop, we want to prevent the user from logging in as root,
  // so that we don't destroy the profile. Now that we have some minimal ui
  // initialized, check to see if we're running as root and bail if we are.
  if (getuid() != 0)
    return;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUserDataDir))
    return;

  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_REFUSE_TO_RUN_AS_ROOT, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_REFUSE_TO_RUN_AS_ROOT_2, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  chrome::ShowWarningMessageBox(NULL, title, message);

  // Avoids gpu_process_transport_factory.cc(153)] Check failed:
  // per_compositor_data_.empty() when quit is chosen.
  base::RunLoop().RunUntilIdle();

  exit(EXIT_FAILURE);
}
