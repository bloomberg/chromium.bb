// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"

#include "ash/accelerators/accelerator_controller_delegate_classic.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/highlighter/highlighter_selection_observer.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_port_classic.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/utility/screenshot_controller.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

constexpr int kMetalayerSelectionDelayMs = 600;

class VoiceInteractionSelectionObserver
    : public ash::HighlighterSelectionObserver {
 public:
  explicit VoiceInteractionSelectionObserver(Profile* profile)
      : profile_(profile) {}
  ~VoiceInteractionSelectionObserver() override = default;

 private:
  void HandleSelection(const gfx::Rect& rect) override {
    // Delay the actual voice interaction service invocation for better
    // visual synchronization with the metalayer animation.
    delay_timer_ = base::MakeUnique<base::Timer>(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMetalayerSelectionDelayMs),
        base::Bind(&VoiceInteractionSelectionObserver::ReportSelection,
                   base::Unretained(this), rect),
        false /* not repeating */);
    delay_timer_->Reset();
  }

  void ReportSelection(const gfx::Rect& rect) {
    auto* framework =
        arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
            profile_);
    if (!framework)
      return;
    framework->StartSessionFromUserInteraction(rect);
  }

  Profile* const profile_;  // Owned by ProfileManager.

  std::unique_ptr<base::Timer> delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionSelectionObserver);
};

PaletteDelegateChromeOS::PaletteDelegateChromeOS() : weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

PaletteDelegateChromeOS::~PaletteDelegateChromeOS() {
  if (highlighter_selection_observer_)
    ash::Shell::Get()->highlighter_controller()->SetObserver(nullptr);
}

std::unique_ptr<PaletteDelegateChromeOS::EnableListenerSubscription>
PaletteDelegateChromeOS::AddPaletteEnableListener(
    const EnableListener& on_state_changed) {
  auto subscription = palette_enabled_callback_list_.Add(on_state_changed);
  OnPaletteEnabledPrefChanged();
  return subscription;
}

void PaletteDelegateChromeOS::CreateNote() {
  if (!profile_)
    return;

  chromeos::NoteTakingHelper::Get()->LaunchAppForNewNote(profile_,
                                                         base::FilePath());
}

bool PaletteDelegateChromeOS::HasNoteApp() {
  if (!profile_)
    return false;

  return chromeos::NoteTakingHelper::Get()->IsAppAvailable(profile_);
}

void PaletteDelegateChromeOS::ActiveUserChanged(
    const user_manager::User* active_user) {
  SetProfile(ProfileHelper::Get()->GetProfileByUser(active_user));
}

void PaletteDelegateChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetActiveUserProfile());

      // Add a session state observer to be able to monitor session changes.
      if (!session_state_observer_.get()) {
        session_state_observer_.reset(
            new user_manager::ScopedUserSessionStateObserver(this));
      }
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Update |profile_| when exiting a session or shutting down.
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_ == profile)
        SetProfile(nullptr);
      break;
    }
  }
}

void PaletteDelegateChromeOS::OnPaletteEnabledPrefChanged() {
  if (profile_) {
    palette_enabled_callback_list_.Notify(
        profile_->GetPrefs()->GetBoolean(prefs::kEnableStylusTools));
  }
}

void PaletteDelegateChromeOS::SetProfile(Profile* profile) {
  profile_ = profile;
  pref_change_registrar_.reset();
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kEnableStylusTools,
      base::Bind(&PaletteDelegateChromeOS::OnPaletteEnabledPrefChanged,
                 base::Unretained(this)));

  // Run listener with new pref value, if any.
  OnPaletteEnabledPrefChanged();
}

void PaletteDelegateChromeOS::OnPartialScreenshotDone(
    const base::Closure& then) {
  if (then)
    then.Run();
}

bool PaletteDelegateChromeOS::ShouldAutoOpenPalette() {
  if (!profile_)
    return false;

  return profile_->GetPrefs()->GetBoolean(prefs::kLaunchPaletteOnEjectEvent);
}

bool PaletteDelegateChromeOS::ShouldShowPalette() {
  if (!profile_)
    return false;

  return profile_->GetPrefs()->GetBoolean(prefs::kEnableStylusTools);
}

void PaletteDelegateChromeOS::TakeScreenshot() {
  auto* screenshot_delegate = ash::ShellPortClassic::Get()
                                  ->accelerator_controller_delegate()
                                  ->screenshot_delegate();
  screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
}

void PaletteDelegateChromeOS::TakePartialScreenshot(const base::Closure& done) {
  auto* screenshot_controller = ash::Shell::Get()->screenshot_controller();
  auto* screenshot_delegate = ash::ShellPortClassic::Get()
                                  ->accelerator_controller_delegate()
                                  ->screenshot_delegate();
  screenshot_controller->set_pen_events_only(true);
  screenshot_controller->StartPartialScreenshotSession(
      screenshot_delegate, false /* draw_overlay_immediately */);
  screenshot_controller->set_on_screenshot_session_done(
      base::Bind(&PaletteDelegateChromeOS::OnPartialScreenshotDone,
                 weak_factory_.GetWeakPtr(), done));
}

void PaletteDelegateChromeOS::CancelPartialScreenshot() {
  ash::Shell::Get()->screenshot_controller()->CancelScreenshotSession();
}

bool PaletteDelegateChromeOS::IsMetalayerSupported() {
  if (!profile_)
    return false;
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(profile_);
  return service && service->IsMetalayerSupported();
}

void PaletteDelegateChromeOS::ShowMetalayer(const base::Closure& closed) {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(profile_);
  if (!service) {
    if (!closed.is_null())
      closed.Run();
    return;
  }
  service->ShowMetalayer(closed);

  if (!highlighter_selection_observer_) {
    highlighter_selection_observer_ =
        base::MakeUnique<VoiceInteractionSelectionObserver>(profile_);
    ash::Shell::Get()->highlighter_controller()->SetObserver(
        highlighter_selection_observer_.get());
  }
  ash::Shell::Get()->highlighter_controller()->SetEnabled(true);
}

void PaletteDelegateChromeOS::HideMetalayer() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(profile_);
  if (!service)
    return;
  service->HideMetalayer();

  ash::Shell::Get()->highlighter_controller()->SetEnabled(false);
}

}  // namespace chromeos
