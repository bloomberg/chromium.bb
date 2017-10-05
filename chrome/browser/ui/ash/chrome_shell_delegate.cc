// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <stddef.h>

#include <limits>
#include <vector>

#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility_delegate.h"
#include "ash/accessibility_types.h"
#include "ash/content/gpu_support_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/chromeos/background/ash_wallpaper_delegate.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"
#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/session_util.h"
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
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_constants.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_constants.h"

using chromeos::AccessibilityManager;

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

void InitAfterFirstSessionStart() {
  // Restore focus after the user session is started.  It's needed because some
  // windows can be opened in background while login UI is still active because
  // we currently restore browser windows before login UI is deleted.
  aura::Window::Windows mru_list =
      ash::Shell::Get()->mru_window_tracker()->BuildMruWindowList();
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
    ash::Shell::Get()->AddShellObserver(AccessibilityManager::Get());
  }
  ~AccessibilityDelegateImpl() override {
    ash::Shell::Get()->RemoveShellObserver(AccessibilityManager::Get());
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

  void SetMagnifierEnabled(bool enabled) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  }

  bool IsMagnifierEnabled() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
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

  void SetStickyKeysEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->EnableStickyKeys(enabled);
  }

  bool IsStickyKeysEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsStickyKeysEnabled();
  }

  void SetTapDraggingEnabled(bool enabled) override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->EnableTapDragging(enabled);
  }

  bool IsTapDraggingEnabled() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->IsTapDraggingEnabled();
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

  void OnTwoFingerTouchStart() override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->OnTwoFingerTouchStart();
  }

  void OnTwoFingerTouchStop() override {
    DCHECK(AccessibilityManager::Get());
    AccessibilityManager::Get()->OnTwoFingerTouchStop();
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
    : networking_config_delegate_(
          base::MakeUnique<chromeos::NetworkingConfigDelegateChromeos>()) {
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {}

service_manager::Connector* ChromeShellDelegate::GetShellConnector() const {
  return content::ServiceManagerConnection::GetForProcess()->GetConnector();
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
  // TODO: port to mash. http://crbug.com/678949.
  if (chromeos::GetAshConfig() == ash::Config::MASH)
    return;

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

void ChromeShellDelegate::OpenUrlFromArc(const GURL& url) {
  if (!url.is_valid())
    return;

  GURL url_to_open = url;
  if (url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kContentScheme)) {
    // Chrome cannot open this URL. Read the contents via ARC content file
    // system with an external file URL.
    url_to_open = arc::ArcUrlToExternalFileUrl(url_to_open);
  }

  // If the url is for system settings, show the settings in a system tray
  // instead of a browser tab.
  if (url_to_open.GetContent() == "settings" &&
      (url_to_open.SchemeIs(url::kAboutScheme) ||
       url_to_open.SchemeIs(content::kChromeUIScheme))) {
    ash::Shell::Get()->system_tray_controller()->ShowSettings();
    return;
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

std::unique_ptr<keyboard::KeyboardUI> ChromeShellDelegate::CreateKeyboardUI() {
  return base::MakeUnique<ChromeKeyboardUI>(
      ProfileManager::GetActiveUserProfile());
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateImpl;
}

std::unique_ptr<ash::PaletteDelegate>
ChromeShellDelegate::CreatePaletteDelegate() {
  return base::MakeUnique<chromeos::PaletteDelegateChromeOS>();
}

ash::NetworkingConfigDelegate*
ChromeShellDelegate::GetNetworkingConfigDelegate() {
  return networking_config_delegate_.get();
}

std::unique_ptr<ash::WallpaperDelegate>
ChromeShellDelegate::CreateWallpaperDelegate() {
  return base::WrapUnique(chromeos::CreateWallpaperDelegate());
}

ui::InputDeviceControllerClient*
ChromeShellDelegate::GetInputDeviceControllerClient() {
  return g_browser_process->platform_part()->GetInputDeviceControllerClient();
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      if (!chromeos::ProfileHelper::IsSigninProfile(profile) &&
          !chromeos::ProfileHelper::IsLockScreenAppProfile(profile) &&
          !profile->IsGuestSession() && !profile->IsSupervised()) {
        // Start the error notifier services to show auth/sync notifications.
        SigninErrorNotifierFactory::GetForProfile(profile);
        SyncErrorNotifierFactory::GetForProfile(profile);
      }
      // Do not use chrome::NOTIFICATION_PROFILE_ADDED because the
      // profile is not fully initialized by user_manager.  Use
      // chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED instead.
      if (ChromeLauncherController::instance()) {
        ChromeLauncherController::instance()->OnUserProfileReadyToSwitch(
            profile);
      }
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
