// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <stddef.h>

#include <limits>

#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/content/gpu_support_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/background/ash_wallpaper_delegate.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/ui/accessibility_focus_ring_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/service_manager_connection.h"
#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using chromeos::AccessibilityManager;

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

void InitAfterFirstSessionStart() {
  // Restore focus after the user session is started.  It's needed because some
  // windows can be opened in background while login UI is still active because
  // we currently restore browser windows before login UI is deleted.
  aura::Window::Windows mru_list = ash::WmWindow::ToAuraWindows(
      ash::WmShell::Get()->mru_window_tracker()->BuildMruWindowList());
  if (!mru_list.empty())
    mru_list.front()->Focus();

  // Enable magnifier scroll keys as there may be no mouse cursor in kiosk mode.
  ash::MagnifierKeyScroller::SetEnabled(chrome::IsRunningInForcedAppMode());

  // Enable long press action to toggle spoken feedback with hotrod
  // remote which can't handle shortcut.
  ash::SpokenFeedbackToggler::SetEnabled(chrome::IsRunningInForcedAppMode());
}

class AccessibilityDelegateImpl : public ash::AccessibilityDelegate {
 public:
  AccessibilityDelegateImpl() {
    ash::WmShell::Get()->AddShellObserver(AccessibilityManager::Get());
  }
  ~AccessibilityDelegateImpl() override {
    ash::WmShell::Get()->RemoveShellObserver(AccessibilityManager::Get());
  }

  void ToggleHighContrast() override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->EnableHighContrast(
        !AccessibilityManager::Get()->IsHighContrastEnabled());
  }

  bool IsSpokenFeedbackEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
  }

  void ToggleSpokenFeedback(
      ash::AccessibilityNotificationVisibility notify) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->ToggleSpokenFeedback(notify);
  }

  bool IsHighContrastEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsHighContrastEnabled();
  }

  void SetMagnifierEnabled(bool enabled) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  }

  void SetMagnifierType(ash::MagnifierType type) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierType(type);
  }

  bool IsMagnifierEnabled() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  }

  ash::MagnifierType GetMagnifierType() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->GetMagnifierType();
  }

  void SetLargeCursorEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->EnableLargeCursor(enabled);
  }

  bool IsLargeCursorEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsLargeCursorEnabled();
  }

  void SetAutoclickEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->EnableAutoclick(enabled);
  }

  bool IsAutoclickEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsAutoclickEnabled();
  }

  void SetVirtualKeyboardEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
  }

  bool IsVirtualKeyboardEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsVirtualKeyboardEnabled();
  }

  void SetMonoAudioEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->EnableMonoAudio(enabled);
  }

  bool IsMonoAudioEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsMonoAudioEnabled();
  }

  void SetCaretHighlightEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->SetCaretHighlightEnabled(enabled);
  }

  bool IsCaretHighlightEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsCaretHighlightEnabled();
  }

  void SetCursorHighlightEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->SetCursorHighlightEnabled(enabled);
  }

  bool IsCursorHighlightEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsCursorHighlightEnabled();
  }

  void SetFocusHighlightEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->SetFocusHighlightEnabled(enabled);
  }

  bool IsFocusHighlightEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsFocusHighlightEnabled();
  }

  void SetSelectToSpeakEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
  }

  bool IsSelectToSpeakEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsSelectToSpeakEnabled();
  }

  void SetSwitchAccessEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->SetSwitchAccessEnabled(enabled);
  }

  bool IsSwitchAccessEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsSwitchAccessEnabled();
  }

  bool ShouldShowAccessibilityMenu() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
  }

  bool IsBrailleDisplayConnected() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsBrailleDisplayConnected();
  }

  void SilenceSpokenFeedback() const override {
    TtsController::GetInstance()->Stop();
  }

  void ClearFocusHighlight() const override {
    chromeos::AccessibilityFocusRingController::GetInstance()->SetFocusRing(
        std::vector<gfx::Rect>(),
        chromeos::AccessibilityFocusRingController::PERSIST_FOCUS_RING);
  }

  void SaveScreenMagnifierScale(double scale) override {
    if (chromeos::MagnificationManager::Get())
      chromeos::MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
  }

  double GetSavedScreenMagnifierScale() override {
    if (chromeos::MagnificationManager::Get()) {
      return chromeos::MagnificationManager::Get()
          ->GetSavedScreenMagnifierScale();
    }
    return std::numeric_limits<double>::min();
  }

  void TriggerAccessibilityAlert(ash::AccessibilityAlert alert) override {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (profile) {
      int msg = 0;
      switch (alert) {
        case ash::A11Y_ALERT_CAPS_ON:
          msg = IDS_A11Y_ALERT_CAPS_ON;
          break;
        case ash::A11Y_ALERT_CAPS_OFF:
          msg = IDS_A11Y_ALERT_CAPS_OFF;
          break;
        case ash::A11Y_ALERT_SCREEN_ON:
          // Enable automation manager when alert is screen-on, as it is
          // previously disabled by alert screen-off.
          SetAutomationManagerEnabled(profile, true);
          msg = IDS_A11Y_ALERT_SCREEN_ON;
          break;
        case ash::A11Y_ALERT_SCREEN_OFF:
          msg = IDS_A11Y_ALERT_SCREEN_OFF;
          break;
        case ash::A11Y_ALERT_WINDOW_NEEDED:
          msg = IDS_A11Y_ALERT_WINDOW_NEEDED;
          break;
        case ash::A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED:
          msg = IDS_A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED;
          break;
        case ash::A11Y_ALERT_NONE:
          msg = 0;
          break;
      }

      if (msg) {
        AutomationManagerAura::GetInstance()->HandleAlert(
            profile, l10n_util::GetStringUTF8(msg));
        // After handling the alert, if the alert is screen-off, we should
        // disable automation manager to handle any following a11y events.
        if (alert == ash::A11Y_ALERT_SCREEN_OFF)
          SetAutomationManagerEnabled(profile, false);
      }
    }
  }

  ash::AccessibilityAlert GetLastAccessibilityAlert() override {
    return ash::A11Y_ALERT_NONE;
  }

  bool ShouldToggleSpokenFeedbackViaTouch() override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->ShouldToggleSpokenFeedbackViaTouch();
  }

  void PlaySpokenFeedbackToggleCountdown(int tick_count) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->PlaySpokenFeedbackToggleCountdown(tick_count);
  }

  void PlayEarcon(int sound_key) override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->PlayEarcon(
        sound_key, chromeos::PlaySoundOption::SPOKEN_FEEDBACK_ENABLED);
  }

  base::TimeDelta PlayShutdownSound() const override {
    return AccessibilityManager::Get()->PlayShutdownSound();
  }

  void HandleAccessibilityGesture(ui::AXGesture gesture) override {
    AccessibilityManager::Get()->HandleAccessibilityGesture(gesture);
  }

 private:
  void SetAutomationManagerEnabled(content::BrowserContext* context,
                                   bool enabled) {
    DCHECK(context);
    AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
    if (enabled)
      manager->Enable(context);
    else
      manager->Disable();
  }

  DISALLOW_COPY_AND_ASSIGN(AccessibilityDelegateImpl);
};

}  // namespace

ChromeShellDelegate::ChromeShellDelegate()
    : shelf_delegate_(NULL) {
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
}

service_manager::Connector* ChromeShellDelegate::GetShellConnector() const {
  return content::ServiceManagerConnection::GetForProcess()->GetConnector();
}

bool ChromeShellDelegate::IsMultiProfilesEnabled() const {
  if (!profiles::IsMultipleProfilesEnabled())
    return false;
  // If there is a user manager, we need to see that we can at least have 2
  // simultaneous users to allow this feature.
  if (!user_manager::UserManager::IsInitialized())
    return false;
  size_t admitted_users_to_be_added =
      user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile().size();
  size_t logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers().size();
  if (!logged_in_users) {
    // The shelf gets created on the login screen and as such we have to create
    // all multi profile items of the the system tray menu before the user logs
    // in. For special cases like Kiosk mode and / or guest mode this isn't a
    // problem since either the browser gets restarted and / or the flag is not
    // allowed, but for an "ephermal" user (see crbug.com/312324) it is not
    // decided yet if they could add other users to their session or not.
    // TODO(skuhne): As soon as the issue above needs to be resolved, this logic
    // should change.
    logged_in_users = 1;
  }
  return admitted_users_to_be_added + logged_in_users > 1;
}

bool ChromeShellDelegate::IsIncognitoAllowed() const {
  return AccessibilityManager::Get()->IsIncognitoAllowed();
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
}

bool ChromeShellDelegate::CanShowWindowForUser(ash::WmWindow* window) const {
  return ::CanShowWindowForUser(ash::WmWindow::GetAuraWindow(window),
                                base::Bind(&GetActiveBrowserContext));
}

bool ChromeShellDelegate::IsForceMaximizeOnFirstRun() const {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (user) {
    return chromeos::ProfileHelper::Get()
        ->GetProfileByUser(user)
        ->GetPrefs()
        ->GetBoolean(prefs::kForceMaximizeOnFirstRun);
  }
  return false;
}

void ChromeShellDelegate::PreInit() {
  bool first_run_after_boot = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
  chromeos::LoadDisplayPreferences(first_run_after_boot);
  // Object owns itself, and deletes itself when Observer::OnShutdown is called:
  new policy::DisplayRotationDefaultHandler();
  // Set the observer now so that we can save the initial state
  // in Shell::Init.
  display_configuration_observer_.reset(
      new chromeos::DisplayConfigurationObserver());
}

void ChromeShellDelegate::PreShutdown() {
  display_configuration_observer_.reset();
}

void ChromeShellDelegate::Exit() {
  chrome::AttemptUserExit();
}

void ChromeShellDelegate::OpenUrlFromArc(const GURL& url) {
  if (!url.is_valid())
    return;

  GURL url_to_open = url;
  if (url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kContentScheme)) {
    // Chrome cannot open this URL. Read the contents via ARC content file
    // system with an external file URL.
    url_to_open = arc::ArcUrlToExternalFileUrl(url_to_open);
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::AddSelectedTabWithURL(
      displayer.browser(), url_to_open,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));

  // Since the ScopedTabbedBrowserDisplayer does not guarantee that the
  // browser will be shown on the active desktop, we ensure the visibility.
  multi_user_util::MoveWindowToCurrentDesktop(
      displayer.browser()->window()->GetNativeWindow());
}

ash::ShelfDelegate* ChromeShellDelegate::CreateShelfDelegate(
    ash::ShelfModel* model) {
  if (!shelf_delegate_) {
    shelf_delegate_ = new ChromeLauncherControllerImpl(nullptr, model);
    shelf_delegate_->Init();
  }
  return shelf_delegate_;
}

ui::MenuModel* ChromeShellDelegate::CreateContextMenu(
    ash::WmShelf* wm_shelf,
    const ash::ShelfItem* item) {
  // Don't show context menu for exclusive app runtime mode.
  if (chrome::IsRunningInAppMode())
    return nullptr;

  // No context menu before |shelf_delegate_| is created. This is possible
  // now because CreateShelfDelegate is called by session state change
  // via mojo asynchronously. Context menu could be triggered when the
  // mojo message is still in-fly and crashes.
  if (!shelf_delegate_)
    return nullptr;

  return LauncherContextMenu::Create(shelf_delegate_, item, wm_shelf);
}

ash::GPUSupport* ChromeShellDelegate::CreateGPUSupport() {
  // Chrome uses real GPU support.
  return new ash::GPUSupportImpl;
}

base::string16 ChromeShellDelegate::GetProductName() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void ChromeShellDelegate::OpenKeyboardShortcutHelpPage() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = chrome::FindTabbedBrowser(profile, false);

  if (!browser) {
    browser = new Browser(Browser::CreateParams(profile, true));
    browser->window()->Show();
  }

  browser->window()->Activate();

  chrome::NavigateParams params(browser, GURL(kKeyboardShortcutHelpPageUrl),
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  chrome::Navigate(&params);
}

gfx::Image ChromeShellDelegate::GetDeprecatedAcceleratorImage() const {
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_BLUETOOTH_KEYBOARD);
}

bool ChromeShellDelegate::IsTouchscreenEnabledInPrefs(
    bool use_local_state) const {
  return chromeos::system::InputDeviceSettings::Get()
      ->IsTouchscreenEnabledInPrefs(use_local_state);
}

void ChromeShellDelegate::SetTouchscreenEnabledInPrefs(bool enabled,
                                                       bool use_local_state) {
  chromeos::system::InputDeviceSettings::Get()->SetTouchscreenEnabledInPrefs(
      enabled, use_local_state);
}

void ChromeShellDelegate::UpdateTouchscreenStatusFromPrefs() {
  chromeos::system::InputDeviceSettings::Get()
      ->UpdateTouchscreenStatusFromPrefs();
}

void ChromeShellDelegate::ToggleTouchpad() {
  chromeos::system::InputDeviceSettings::Get()->ToggleTouchpad();
}

keyboard::KeyboardUI* ChromeShellDelegate::CreateKeyboardUI() {
  return new ChromeKeyboardUI(ProfileManager::GetActiveUserProfile());
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegateChromeos;
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateImpl;
}

std::unique_ptr<ash::PaletteDelegate>
ChromeShellDelegate::CreatePaletteDelegate() {
  return base::MakeUnique<chromeos::PaletteDelegateChromeOS>();
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return chromeos::CreateSystemTrayDelegate();
}

std::unique_ptr<ash::WallpaperDelegate>
ChromeShellDelegate::CreateWallpaperDelegate() {
  return base::WrapUnique(chromeos::CreateWallpaperDelegate());
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      if (!chromeos::ProfileHelper::IsSigninProfile(profile) &&
          !profile->IsGuestSession() && !profile->IsSupervised()) {
        // Start the error notifier services to show auth/sync notifications.
        SigninErrorNotifierFactory::GetForProfile(profile);
        SyncErrorNotifierFactory::GetForProfile(profile);
      }
      // Do not use chrome::NOTIFICATION_PROFILE_ADDED because the
      // profile is not fully initialized by user_manager.  Use
      // chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED instead.
      if (shelf_delegate_)
        shelf_delegate_->OnUserProfileReadyToSwitch(profile);
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED:
      // InitAfterFirstSessionStart() should only be called once upon system
      // start.
      if (user_manager::UserManager::Get()->GetLoggedInUsers().size() < 2)
        InitAfterFirstSessionStart();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void ChromeShellDelegate::PlatformInit() {
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
}
