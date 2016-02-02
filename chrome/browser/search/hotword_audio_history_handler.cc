// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_audio_history_handler.h"

#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/pref_service.h"

using extensions::BrowserContextKeyedAPIFactory;
using extensions::HotwordPrivateEventService;

// Max number of hours between audio history checks.
static const int kHoursUntilNextAudioHistoryCheck = 24;

HotwordAudioHistoryHandler::HotwordAudioHistoryHandler(
    content::BrowserContext* context,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      profile_(Profile::FromBrowserContext(context)),
      weak_ptr_factory_(this) {
}

HotwordAudioHistoryHandler::~HotwordAudioHistoryHandler() {
}

history::WebHistoryService* HotwordAudioHistoryHandler::GetWebHistory() {
  return WebHistoryServiceFactory::GetForProfile(profile_);
}

void HotwordAudioHistoryHandler::UpdateAudioHistoryState() {
  GetAudioHistoryEnabled(
      base::Bind(&HotwordAudioHistoryHandler::UpdateLocalPreference,
                 weak_ptr_factory_.GetWeakPtr()));
  // Set the function to update in a day.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HotwordAudioHistoryHandler::UpdateAudioHistoryState,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromHours(kHoursUntilNextAudioHistoryCheck));
}

void HotwordAudioHistoryHandler::UpdateLocalPreference(
    bool success, bool new_enabled_value) {
  if (success) {
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(prefs::kHotwordAudioLoggingEnabled, new_enabled_value);
  }
}

void HotwordAudioHistoryHandler::GetAudioHistoryEnabled(
    const HotwordAudioHistoryCallback& callback) {
  history::WebHistoryService* web_history = GetWebHistory();
  if (web_history) {
    web_history->GetAudioHistoryEnabled(
        base::Bind(&HotwordAudioHistoryHandler::GetAudioHistoryComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  } else {
    // If web_history is null, the user is not signed in. Set the opt-in value
    // to the last known value and run the callback with false for success.
    PrefService* prefs = profile_->GetPrefs();
    callback.Run(false, prefs->GetBoolean(prefs::kHotwordAudioLoggingEnabled));
  }
}

void HotwordAudioHistoryHandler::SetAudioHistoryEnabled(
    const bool enabled,
    const HotwordAudioHistoryCallback& callback) {
  history::WebHistoryService* web_history = GetWebHistory();
  if (web_history) {
    web_history->SetAudioHistoryEnabled(
        enabled,
        base::Bind(&HotwordAudioHistoryHandler::SetAudioHistoryComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   enabled,
                   callback));
  } else {
    // If web_history is null, run the callback with false for success
    // and return the last known value for the opt-in pref.
    PrefService* prefs = profile_->GetPrefs();
    callback.Run(false, prefs->GetBoolean(prefs::kHotwordAudioLoggingEnabled));
  }
}

void HotwordAudioHistoryHandler::GetAudioHistoryComplete(
    const HotwordAudioHistoryCallback& callback,
    bool success, bool new_enabled_value) {
  // Initialize value to the last known value of the audio history pref.
  PrefService* prefs = profile_->GetPrefs();
  bool value = prefs->GetBoolean(prefs::kHotwordAudioLoggingEnabled);
  // If the call was successful, use the new value for updates.
  if (success) {
    value = new_enabled_value;
    prefs->SetBoolean(prefs::kHotwordAudioLoggingEnabled, value);
    // If the setting is now turned off, always on should also be turned off,
    // and the speaker model should be deleted.
    if (!value) {
      if (prefs->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled)) {
        prefs->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, false);
        HotwordPrivateEventService* event_service =
            BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(
                profile_);
        if (event_service)
          event_service->OnDeleteSpeakerModel();
      }
    }
  }
  callback.Run(success, value);
}

void HotwordAudioHistoryHandler::SetAudioHistoryComplete(
    bool new_enabled_value,
    const HotwordAudioHistoryCallback& callback,
    bool success, bool callback_enabled_value) {
  UpdateLocalPreference(success, new_enabled_value);
  callback.Run(success, new_enabled_value);
}
