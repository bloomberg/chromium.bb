// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <stddef.h>

#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility_delegate.h"
#include "ash/container_delegate_aura.h"
#include "ash/content/gpu_support_impl.h"
#include "ash/pointer_watcher_delegate_aura.h"
#include "ash/session/session_state_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/media_delegate_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
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
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

void InitAfterFirstSessionStart() {
  // Restore focus after the user session is started.  It's needed because some
  // windows can be opened in background while login UI is still active because
  // we currently restore browser windows before login UI is deleted.
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::MruWindowTracker::WindowList mru_list =
      shell->mru_window_tracker()->BuildMruWindowList();
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
    ash::Shell::GetInstance()->AddShellObserver(
        chromeos::AccessibilityManager::Get());
  }
  ~AccessibilityDelegateImpl() override {
    ash::Shell::GetInstance()->RemoveShellObserver(
        chromeos::AccessibilityManager::Get());
  }

  void ToggleHighContrast() override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->EnableHighContrast(
        !chromeos::AccessibilityManager::Get()->IsHighContrastEnabled());
  }

  bool IsSpokenFeedbackEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
  }

  void ToggleSpokenFeedback(
      ui::AccessibilityNotificationVisibility notify) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->ToggleSpokenFeedback(notify);
  }

  bool IsHighContrastEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsHighContrastEnabled();
  }

  void SetMagnifierEnabled(bool enabled) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  }

  void SetMagnifierType(ui::MagnifierType type) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierType(type);
  }

  bool IsMagnifierEnabled() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  }

  ui::MagnifierType GetMagnifierType() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->GetMagnifierType();
  }

  void SetLargeCursorEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableLargeCursor(enabled);
  }

  bool IsLargeCursorEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsLargeCursorEnabled();
  }

  void SetAutoclickEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableAutoclick(enabled);
  }

  bool IsAutoclickEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsAutoclickEnabled();
  }

  void SetVirtualKeyboardEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableVirtualKeyboard(
        enabled);
  }

  bool IsVirtualKeyboardEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsVirtualKeyboardEnabled();
  }

  void SetMonoAudioEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableMonoAudio(enabled);
  }

  bool IsMonoAudioEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsMonoAudioEnabled();
  }

  void SetCaretHighlightEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->SetCaretHighlightEnabled(enabled);
  }

  bool IsCaretHighlightEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsCaretHighlightEnabled();
  }

  void SetCursorHighlightEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->SetCursorHighlightEnabled(enabled);
  }

  bool IsCursorHighlightEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsCursorHighlightEnabled();
  }

  void SetFocusHighlightEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->SetFocusHighlightEnabled(enabled);
  }

  bool IsFocusHighlightEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsFocusHighlightEnabled();
  }

  void SetSelectToSpeakEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
  }

  bool IsSelectToSpeakEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsSelectToSpeakEnabled();
  }

  void SetSwitchAccessEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->SetSwitchAccessEnabled(enabled);
  }

  bool IsSwitchAccessEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsSwitchAccessEnabled();
  }

  bool ShouldShowAccessibilityMenu() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
  }

  bool IsBrailleDisplayConnected() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsBrailleDisplayConnected();
  }

  void SilenceSpokenFeedback() const override {
    TtsController::GetInstance()->Stop();
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

  void TriggerAccessibilityAlert(ui::AccessibilityAlert alert) override {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (profile) {
      switch (alert) {
        case ui::A11Y_ALERT_WINDOW_NEEDED: {
          AutomationManagerAura::GetInstance()->HandleAlert(
              profile, l10n_util::GetStringUTF8(IDS_A11Y_ALERT_WINDOW_NEEDED));
          break;
        }
        case ui::A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED: {
          AutomationManagerAura::GetInstance()->HandleAlert(
              profile, l10n_util::GetStringUTF8(
                           IDS_A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED));
          break;
        }
        case ui::A11Y_ALERT_NONE:
          break;
      }
    }
  }

  ui::AccessibilityAlert GetLastAccessibilityAlert() override {
    return ui::A11Y_ALERT_NONE;
  }

  void PlayEarcon(int sound_key) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->PlayEarcon(sound_key);
  }

  base::TimeDelta PlayShutdownSound() const override {
    return chromeos::AccessibilityManager::Get()->PlayShutdownSound();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDelegateImpl);
};

}  // namespace

ChromeShellDelegate::ChromeShellDelegate()
    : shelf_delegate_(NULL) {
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
}

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
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
    // decided yet if he could add other users to his session or not.
    // TODO(skuhne): As soon as the issue above needs to be resolved, this logic
    // should change.
    logged_in_users = 1;
  }
  return admitted_users_to_be_added + logged_in_users > 1;
}

bool ChromeShellDelegate::IsIncognitoAllowed() const {
  return chromeos::AccessibilityManager::Get()->IsIncognitoAllowed();
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
}

bool ChromeShellDelegate::CanShowWindowForUser(aura::Window* window) const {
  return ::CanShowWindowForUser(window, base::Bind(&GetActiveBrowserContext));
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
  chromeos::LoadDisplayPreferences(IsFirstRunAfterBoot());
  // Object owns itself, and deletes itself when Observer::OnShutdown is called:
  new policy::DisplayRotationDefaultHandler();
  // Set the observer now so that we can save the initial state
  // in Shell::Init.
  display_configuration_observer_.reset(
      new chromeos::DisplayConfigurationObserver());

  chrome_user_metrics_recorder_.reset(new ChromeUserMetricsRecorder);
}

void ChromeShellDelegate::PreShutdown() {
  display_configuration_observer_.reset();

  chrome_user_metrics_recorder_.reset();
}

void ChromeShellDelegate::Exit() {
  chrome::AttemptUserExit();
}

void ChromeShellDelegate::OpenUrl(const GURL& url) {
  if (!url.is_valid())
    return;

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::AddSelectedTabWithURL(displayer.browser(), url,
                                ui::PAGE_TRANSITION_LINK);

  // Since the ScopedTabbedBrowserDisplayer does not guarantee that the
  // browser will be shown on the active desktop, we ensure the visibility.
  multi_user_util::MoveWindowToCurrentDesktop(
      displayer.browser()->window()->GetNativeWindow());
}

app_list::AppListPresenter* ChromeShellDelegate::GetAppListPresenter() {
  DCHECK(ash::Shell::HasInstance());
  return AppListServiceAsh::GetInstance()->GetAppListPresenter();
}

ash::ShelfDelegate* ChromeShellDelegate::CreateShelfDelegate(
    ash::ShelfModel* model) {
  if (!shelf_delegate_) {
    shelf_delegate_ = ChromeLauncherController::CreateInstance(NULL, model);
    shelf_delegate_->Init();
  }
  return shelf_delegate_;
}

std::unique_ptr<ash::ContainerDelegate>
ChromeShellDelegate::CreateContainerDelegate() {
  return base::WrapUnique(new ash::ContainerDelegateAura);
}

std::unique_ptr<ash::PointerWatcherDelegate>
ChromeShellDelegate::CreatePointerWatcherDelegate() {
  return base::WrapUnique(new ash::PointerWatcherDelegateAura);
}

ui::MenuModel* ChromeShellDelegate::CreateContextMenu(
    ash::Shelf* shelf,
    const ash::ShelfItem* item) {
  DCHECK(shelf_delegate_);
  // Don't show context menu for exclusive app runtime mode.
  if (chrome::IsRunningInAppMode())
    return nullptr;

  return LauncherContextMenu::Create(shelf_delegate_, item, shelf);
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
    browser = new Browser(Browser::CreateParams(profile));
    browser->window()->Show();
  }

  browser->window()->Activate();

  chrome::NavigateParams params(browser, GURL(kKeyboardShortcutHelpPageUrl),
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = SINGLETON_TAB;
  chrome::Navigate(&params);
}

gfx::Image ChromeShellDelegate::GetDeprecatedAcceleratorImage() const {
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_BLUETOOTH_KEYBOARD);
}

void ChromeShellDelegate::ToggleTouchpad() {
  chromeos::system::InputDeviceSettings::Get()->ToggleTouchpad();
}

void ChromeShellDelegate::ToggleTouchscreen() {
  chromeos::system::InputDeviceSettings::Get()->ToggleTouchscreen();
}

keyboard::KeyboardUI* ChromeShellDelegate::CreateKeyboardUI() {
  return new ChromeKeyboardUI(ProfileManager::GetActiveUserProfile());
}

void ChromeShellDelegate::VirtualKeyboardActivated(bool activated) {
  FOR_EACH_OBSERVER(ash::VirtualKeyboardStateObserver,
                    keyboard_state_observer_list_,
                    OnVirtualKeyboardStateChanged(activated));
}

void ChromeShellDelegate::AddVirtualKeyboardStateObserver(
    ash::VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.AddObserver(observer);
}

void ChromeShellDelegate::RemoveVirtualKeyboardStateObserver(
    ash::VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.RemoveObserver(observer);
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegateChromeos;
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateImpl;
}

ash::NewWindowDelegate* ChromeShellDelegate::CreateNewWindowDelegate() {
  return new ChromeNewWindowDelegate;
}

ash::MediaDelegate* ChromeShellDelegate::CreateMediaDelegate() {
  return new MediaDelegateChromeOS;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return chromeos::CreateSystemTrayDelegate();
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
  return chromeos::CreateUserWallpaperDelegate();
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
      ash::Shell::GetInstance()->OnLoginUserProfilePrepared();
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED:
      // InitAfterFirstSessionStart() should only be called once upon system
      // start.
      if (user_manager::UserManager::Get()->GetLoggedInUsers().size() < 2)
        InitAfterFirstSessionStart();
      ash::Shell::GetInstance()->ShowShelf();
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
