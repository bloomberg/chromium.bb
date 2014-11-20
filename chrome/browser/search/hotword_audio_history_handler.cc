// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_audio_history_handler.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/history/web_history_service.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

HotwordAudioHistoryHandler::HotwordAudioHistoryHandler(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      weak_factory_(this) {

  // Poll for current value on init which should happen on first use by
  // way of the hotword service init.
  GetAudioHistoryEnabled();
}

HotwordAudioHistoryHandler::~HotwordAudioHistoryHandler() {
}

void HotwordAudioHistoryHandler::GetAudioHistoryEnabled() {
  history::WebHistoryService* web_history =
    WebHistoryServiceFactory::GetForProfile(profile_);
  if (web_history) {
    web_history->GetAudioHistoryEnabled(
        base::Bind(&HotwordAudioHistoryHandler::AudioHistoryComplete,
                   weak_factory_.GetWeakPtr()));
  } else {
    // If web_history is null, the user is not signed in.
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(prefs::kHotwordAudioHistoryEnabled, false);
  }
}

void HotwordAudioHistoryHandler::SetAudioHistoryEnabled(const bool enabled) {
  history::WebHistoryService* web_history =
  WebHistoryServiceFactory::GetForProfile(profile_);
  if (web_history) {
    web_history->SetAudioHistoryEnabled(
        enabled,
        base::Bind(&HotwordAudioHistoryHandler::AudioHistoryComplete,
                   weak_factory_.GetWeakPtr()));
  }
}

void HotwordAudioHistoryHandler::AudioHistoryComplete(
  bool success, bool new_enabled_value) {
  PrefService* prefs = profile_->GetPrefs();
  // Set preference to false if the call was not successful to err on the safe
  // side.
  prefs->SetBoolean(prefs::kHotwordAudioHistoryEnabled,
                    success && new_enabled_value);
}
