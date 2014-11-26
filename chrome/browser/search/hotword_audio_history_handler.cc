// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_audio_history_handler.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/history/web_history_service.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

using extensions::BrowserContextKeyedAPIFactory;
using extensions::HotwordPrivateEventService;

HotwordAudioHistoryHandler::HotwordAudioHistoryHandler(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      weak_ptr_factory_(this) {
}

HotwordAudioHistoryHandler::~HotwordAudioHistoryHandler() {
}

history::WebHistoryService* HotwordAudioHistoryHandler::GetWebHistory() {
  return WebHistoryServiceFactory::GetForProfile(profile_);
}

void HotwordAudioHistoryHandler::GetAudioHistoryEnabled(
    const HotwordAudioHistoryCallback& callback) {
  history::WebHistoryService* web_history = GetWebHistory();
  if (web_history) {
    web_history->GetAudioHistoryEnabled(
        base::Bind(&HotwordAudioHistoryHandler::AudioHistoryComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  } else {
    // If web_history is null, the user is not signed in so the opt-in
    // should be seen as false. Run the callback with false for success
    // and false for the enabled value.
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(prefs::kHotwordAudioHistoryEnabled, false);
    callback.Run(false, false);
  }
}

void HotwordAudioHistoryHandler::SetAudioHistoryEnabled(
    const bool enabled,
    const HotwordAudioHistoryCallback& callback) {
  history::WebHistoryService* web_history = GetWebHistory();
  if (web_history) {
    web_history->SetAudioHistoryEnabled(
        enabled,
        base::Bind(&HotwordAudioHistoryHandler::AudioHistoryComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  } else {
    // If web_history is null, run the callback with false for success
    // and false for the enabled value.
    callback.Run(false, false);
  }
}

void HotwordAudioHistoryHandler::AudioHistoryComplete(
    const HotwordAudioHistoryCallback& callback,
    bool success, bool new_enabled_value) {
  PrefService* prefs = profile_->GetPrefs();
  // Set preference to false if the call was not successful to err on the safe
  // side.
  bool new_value = success && new_enabled_value;
  prefs->SetBoolean(prefs::kHotwordAudioHistoryEnabled, new_value);

  callback.Run(success, new_value);
}
