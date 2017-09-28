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
      : profile_(profile) {
    ash::Shell::Get()->highlighter_controller()->SetObserver(this);
  }

  ~VoiceInteractionSelectionObserver() override {
    if (ash::Shell::HasInstance() &&
        ash::Shell::Get()->highlighter_controller()) {
      ash::Shell::Get()->highlighter_controller()->SetObserver(nullptr);
    }
  };

  void set_on_selection_done(base::OnceClosure done) {
    on_selection_done_ = std::move(done);
    delay_timer_.reset();
  }

  void set_disable_on_failed_selection(bool disable_on_failed_selection) {
    disable_on_failed_selection_ = disable_on_failed_selection;
  }

  bool start_session_pending() const { return delay_timer_.get(); }

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
    DisableMetalayer();
  }

  void HandleFailedSelection() override {
    if (disable_on_failed_selection_)
      DisableMetalayer();
  }

  void DisableMetalayer() {
    DCHECK(on_selection_done_);
    // This will disable the metalayer tool, which will result in a synchronous
    // call to PaletteDelegateChromeOS::HideMetalayer.
    std::move(on_selection_done_).Run();
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
  base::OnceClosure on_selection_done_;
  bool disable_on_failed_selection_ = true;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionSelectionObserver);
};

PaletteDelegateChromeOS::PaletteDelegateChromeOS() : weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

PaletteDelegateChromeOS::~PaletteDelegateChromeOS() {
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

void PaletteDelegateChromeOS::ShowMetalayer(base::OnceClosure done,
                                            bool via_button) {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(profile_);
  if (!service)
    return;
  service->ShowMetalayer();

  if (!highlighter_selection_observer_) {
    highlighter_selection_observer_ =
        base::MakeUnique<VoiceInteractionSelectionObserver>(profile_);
  }
  highlighter_selection_observer_->set_on_selection_done(std::move(done));
  highlighter_selection_observer_->set_disable_on_failed_selection(via_button);
  ash::Shell::Get()->highlighter_controller()->SetEnabled(true);
}

void PaletteDelegateChromeOS::HideMetalayer() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(profile_);
  if (!service)
    return;

  // ArcVoiceInteractionFrameworkService::HideMetalayer() causes the container
  // to show a toast-like prompt. This toast is redundant and causes unnecessary
  // flicker if the full voice interaction UI is about to be displayed soon.
  // |start_session_pending| is a good signal that the session is about to
  // start, but it is not guaranteed:
  // 1) The user might re-enter the metalayer mode before the timer fires.
  //    In this case the container will keep showing the prompt for the
  //    metalayer mode.
  // 2) The session might fail to start due to a peculiar combination of
  //    failures on the way to the voice interaction UI. This is an open
  //    problem.
  // TODO(kaznacheev) Move this logic under ash when fixing crbug/761120.

  DCHECK(highlighter_selection_observer_);
  if (!highlighter_selection_observer_->start_session_pending())
    service->HideMetalayer();

  ash::Shell::Get()->highlighter_controller()->SetEnabled(false);
}

}  // namespace chromeos
