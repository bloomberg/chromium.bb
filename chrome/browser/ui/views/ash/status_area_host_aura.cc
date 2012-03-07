// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/status_area_host_aura.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/memory_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/ash/multiple_window_indicator_button.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/status/clock_updater.h"
#include "chrome/browser/chromeos/status/status_area_view_chromeos.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "ui/gfx/native_widget_types.h"
#endif

namespace {

// Horizontal padding between the side of the status area and the side of the
// screen in compact mode.
const int kCompactModeHorizontalOffset = 3;

// Vertical padding between the top of the status area and the top of the screen
// when we're displaying either the login/lock screen or a browser window in
// compact mode.
const int kCompactModeLoginAndLockVerticalOffset = 4;
const int kCompactModeBrowserVerticalOffset = 2;

}  // namespace

// static
gfx::Size StatusAreaHostAura::GetCompactModeLoginAndLockOffset() {
  return gfx::Size(kCompactModeHorizontalOffset,
                   kCompactModeLoginAndLockVerticalOffset);
}

// static
gfx::Size StatusAreaHostAura::GetCompactModeBrowserOffset() {
  return gfx::Size(kCompactModeHorizontalOffset,
                   kCompactModeBrowserVerticalOffset);
}

StatusAreaHostAura::StatusAreaHostAura()
    : status_area_widget_(NULL),
      status_area_view_(NULL) {
  BrowserList::AddObserver(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::NotificationService::AllSources());
#if defined(OS_CHROMEOS)
  registrar_.Add(this,
                 chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                 content::NotificationService::AllSources());
#endif
}

StatusAreaHostAura::~StatusAreaHostAura() {
  BrowserList::RemoveObserver(this);
}

StatusAreaView* StatusAreaHostAura::GetStatusArea() {
  return status_area_view_;
}

views::Widget* StatusAreaHostAura::CreateStatusArea() {
  ash::Shell* shell = ash::Shell::GetInstance();
  aura::Window* status_window = shell->GetContainer(
      ash::internal::kShellWindowId_StatusContainer);

  // Create status area view.
  status_area_view_ = new StatusAreaView();

  // Add multiple window indicator button for compact mode. Note this should be
  // the last button added to status area so that it could be right most there.
  if (ash::Shell::GetInstance()->IsWindowModeCompact()) {
    status_area_view_->AddButton(new MultipleWindowIndicatorButton(this),
                                 StatusAreaView::NO_BORDER);
  }

  // Create widget to hold status area view.
  status_area_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  gfx::Size ps = status_area_view_->GetPreferredSize();
  params.bounds = gfx::Rect(0, 0, ps.width(), ps.height());
  params.delegate = status_area_view_;
  params.parent = status_window;
  params.transparent = true;
  status_area_widget_->Init(params);
  status_area_widget_->GetNativeWindow()->SetName("StatusAreaWindow");
  // Turn off focus on creation, otherwise the status area will request focus
  // every time it is shown.
  status_area_widget_->set_focus_on_creation(false);
  status_area_widget_->SetContentsView(status_area_view_);
  status_area_widget_->Show();
  status_area_widget_->GetNativeView()->SetName("StatusAreaView");

  UpdateAppearance();

  return status_area_widget_;
}

void StatusAreaHostAura::AddButtons() {
#if defined(OS_CHROMEOS)
  ClockMenuButton* clock = NULL;
  chromeos::StatusAreaViewChromeos::AddChromeosButtons(status_area_view_,
                                                       this,
                                                       &clock);
  if (clock)
    clock_updater_.reset(new ClockUpdater(clock));
#else
#if defined(OS_LINUX)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kMemoryWidget))
    status_area_view_->AddButton(new MemoryMenuButton(this),
                                 StatusAreaView::NO_BORDER);
#endif
  status_area_view_->AddButton(new ClockMenuButton(this),
                               StatusAreaView::HAS_BORDER);
#endif
}

// StatusAreaButton::Delegate implementation.

bool StatusAreaHostAura::ShouldExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) const {
#if defined(OS_CHROMEOS)
  if (chromeos::StatusAreaViewChromeos::IsLoginMode()) {
    // In login mode network options command means proxy settings dialog.
    return command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS;
  } else {
    return !chromeos::StatusAreaViewChromeos::IsScreenLockMode();
  }
#else
  // TODO(stevenjb): system options for non-chromeos Aura?
  return false;
#endif
}

void StatusAreaHostAura::ExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) {
#if defined(OS_CHROMEOS)
  if (chromeos::StatusAreaViewChromeos::IsBrowserMode()) {
    Profile* profile = ProfileManager::GetDefaultProfile();
    if (browser_defaults::kAlwaysOpenIncognitoWindow &&
        IncognitoModePrefs::ShouldLaunchIncognito(
            *CommandLine::ForCurrentProcess(),
            profile->GetPrefs())) {
      profile = profile->GetOffTheRecordProfile();
    }
    Browser* browser = BrowserList::FindBrowserWithProfile(profile);
    if (!browser)
      browser = Browser::Create(profile);
    switch (command_id) {
      case StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS:
        browser->OpenInternetOptionsDialog();
        break;
      case StatusAreaButton::Delegate::SHOW_LANGUAGE_OPTIONS:
        browser->OpenLanguageOptionsDialog();
        break;
      case StatusAreaButton::Delegate::SHOW_ADVANCED_OPTIONS:
        browser->OpenAdvancedOptionsDialog();
        break;
      default:
        NOTREACHED();
    }
  } else if (chromeos::StatusAreaViewChromeos::IsLoginMode()) {
    if (command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS &&
        chromeos::BaseLoginDisplayHost::default_host()) {
      gfx::NativeWindow native_window =
          chromeos::BaseLoginDisplayHost::default_host()->GetNativeWindow();
      proxy_settings_dialog_.reset(new chromeos::ProxySettingsDialog(
          NULL, native_window));
      proxy_settings_dialog_->Show();
    } else {
      NOTREACHED();
    }
  } else if (chromeos::StatusAreaViewChromeos::IsScreenLockMode()) {
    if (command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS &&
        chromeos::ScreenLocker::default_screen_locker()) {
      gfx::NativeWindow native_window =
          chromeos::ScreenLocker::default_screen_locker()->delegate()->
              GetNativeWindow();
      proxy_settings_dialog_.reset(new chromeos::ProxySettingsDialog(
                                   NULL,
                                   native_window));
      proxy_settings_dialog_->Show();
    } else {
      NOTREACHED();
    }
  }
#endif
}

StatusAreaButton::TextStyle StatusAreaHostAura::GetStatusAreaTextStyle() const {
#if defined(OS_CHROMEOS)
  if (IsLoginOrLockScreenDisplayed())
    return StatusAreaButton::GRAY_PLAIN_LIGHT;
#endif

  if (ash::Shell::GetInstance()->IsWindowModeCompact()) {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return StatusAreaButton::WHITE_HALOED_BOLD;

    ThemeService* theme_service =
        ThemeServiceFactory::GetForProfile(browser->profile());
    if (!theme_service->UsingDefaultTheme())
      return StatusAreaButton::WHITE_HALOED_BOLD;

    return browser->profile()->IsOffTheRecord() ?
        StatusAreaButton::WHITE_PLAIN_BOLD :
        StatusAreaButton::GRAY_EMBOSSED_BOLD;
  } else {
    return StatusAreaButton::WHITE_HALOED_BOLD;
  }
}

void StatusAreaHostAura::ButtonVisibilityChanged(views::View* button_view) {
  if (status_area_view_)
    status_area_view_->UpdateButtonVisibility();
}

void StatusAreaHostAura::OnBrowserSetLastActive(const Browser* browser) {
  UpdateAppearance();
}

void StatusAreaHostAura::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      UpdateAppearance();
      break;
#if defined(OS_CHROMEOS)
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED:
      UpdateAppearance();
      break;
#endif
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

bool StatusAreaHostAura::IsLoginOrLockScreenDisplayed() const {
#if defined(OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->IsUserLoggedIn() &&
      chromeos::system::runtime_environment::IsRunningOnChromeOS())
    return true;

  const chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (locker && locker->locked())
    return true;
#endif

  return false;
}

void StatusAreaHostAura::UpdateAppearance() {
  status_area_view_->UpdateButtonTextStyle();

  gfx::Size offset = IsLoginOrLockScreenDisplayed() ?
      GetCompactModeLoginAndLockOffset() :
      GetCompactModeBrowserOffset();
  ash::Shell::GetInstance()->SetCompactStatusAreaOffset(offset);
}
