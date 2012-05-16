// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

// static
ManagedMode* ManagedMode::GetInstance() {
  return Singleton<ManagedMode>::get();
}

// static
void ManagedMode::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInManagedMode, false);
  // Set the value directly in the PrefService instead of using
  // CommandLinePrefStore so we can change it at runtime.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoManaged))
    GetInstance()->SetInManagedMode(false);
  else if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kManaged))
    GetInstance()->SetInManagedMode(true);
}

// static
bool ManagedMode::IsInManagedMode() {
  return GetInstance()->IsInManagedModeImpl();
}

bool ManagedMode::IsInManagedModeImpl() {
  // |g_browser_process| can be NULL during startup.
  if (!g_browser_process)
    return false;
  // Local State can be NULL during unit tests.
  if (!g_browser_process->local_state())
    return false;
  return g_browser_process->local_state()->GetBoolean(prefs::kInManagedMode);
}

// static
void ManagedMode::EnterManagedMode(Profile* profile,
                                   const EnterCallback& callback) {
  GetInstance()->EnterManagedModeImpl(profile, callback);
}

void ManagedMode::EnterManagedModeImpl(Profile* profile,
                                       const EnterCallback& callback) {
  if (IsInManagedModeImpl()) {
    callback.Run(true);
    return;
  }
  Profile* original_profile = profile->GetOriginalProfile();
  if (!callbacks_.empty()) {
    // We are already in the process of entering managed mode, waiting for
    // browsers to close. Don't allow entering managed mode again for a
    // different profile, and queue the callback for the same profile.
    if (original_profile != managed_profile_)
      callback.Run(false);
    else
      callbacks_.push_back(callback);
    return;
  }

  if (!PlatformConfirmEnter()) {
    callback.Run(false);
    return;
  }
  managed_profile_ = original_profile;
  // Close all other profiles.
  // At this point, we shouldn't be waiting for other browsers to close (yet).
  DCHECK_EQ(0u, browsers_to_close_.size());
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->GetOriginalProfile() != managed_profile_)
      browsers_to_close_.insert(*i);
  }

  if (browsers_to_close_.empty()) {
    SetInManagedMode(true);
    managed_profile_ = NULL;
    callback.Run(true);
    return;
  }
  callbacks_.push_back(callback);
  registrar_.Add(this, content::NOTIFICATION_APP_EXITING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
                 content::NotificationService::AllSources());
  for (std::set<const Browser*>::const_iterator i = browsers_to_close_.begin();
       i != browsers_to_close_.end(); ++i) {
    (*i)->window()->Close();
  }
}

// static
void ManagedMode::LeaveManagedMode() {
  GetInstance()->LeaveManagedModeImpl();
}

void ManagedMode::LeaveManagedModeImpl() {
  bool confirmed = PlatformConfirmLeave();
  if (confirmed)
    SetInManagedMode(false);
}

void ManagedMode::OnBrowserAdded(const Browser* browser) {
  // Return early if we don't have any queued callbacks.
  if (callbacks_.empty())
    return;

  if (browser->profile()->GetOriginalProfile() != managed_profile_)
    FinalizeEnter(false);
}

void ManagedMode::OnBrowserRemoved(const Browser* browser) {
  // Return early if we don't have any queued callbacks.
  if (callbacks_.empty())
    return;

  if (browser->profile()->GetOriginalProfile() == managed_profile_) {
    // Ignore closing browser windows that are in managed mode.
    return;
  }
  size_t count = browsers_to_close_.erase(browser);
  DCHECK_EQ(1u, count);
  if (browsers_to_close_.empty())
    FinalizeEnter(true);
}

ManagedMode::ManagedMode() : managed_profile_(NULL) {
  BrowserList::AddObserver(this);
}

ManagedMode::~ManagedMode() {
  BrowserList::RemoveObserver(this);
  DCHECK(!managed_profile_);
  DCHECK_EQ(0u, callbacks_.size());
  DCHECK_EQ(0u, browsers_to_close_.size());
}

void ManagedMode::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  // Return early if we don't have any queued callbacks.
  if (callbacks_.empty())
    return;

  switch (type) {
    case content::NOTIFICATION_APP_EXITING: {
      FinalizeEnter(false);
      return;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      if (browsers_to_close_.find(browser) != browsers_to_close_.end())
        FinalizeEnter(false);
      return;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void ManagedMode::FinalizeEnter(bool result) {
  if (result)
    SetInManagedMode(true);
  for (std::vector<EnterCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end(); ++it) {
    it->Run(result);
  }
  managed_profile_ = NULL;
  callbacks_.clear();
  browsers_to_close_.clear();
  registrar_.RemoveAll();
}

bool ManagedMode::PlatformConfirmEnter() {
  // TODO(bauerb): Show platform-specific confirmation dialog.
  return true;
}

bool ManagedMode::PlatformConfirmLeave() {
  // TODO(bauerb): Show platform-specific confirmation dialog.
  return true;
}

void ManagedMode::SetInManagedMode(bool in_managed_mode) {
  g_browser_process->local_state()->SetBoolean(prefs::kInManagedMode,
                                               in_managed_mode);
  // This causes the avatar and the profile menu to get updated.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());
}

