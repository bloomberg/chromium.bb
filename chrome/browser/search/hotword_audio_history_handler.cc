// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_audio_history_handler.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

HotwordAudioHistoryHandler::HotwordAudioHistoryHandler(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
    prefs::kHotwordAudioHistoryEnabled,
    base::Bind(&HotwordAudioHistoryHandler::OnAudioHistoryEnabledChanged,
    base::Unretained(this)));

  // Poll for current value on init which should happen on first use by
  // way of the hotword service init.
  GetAudioHistoryEnabled();
}

HotwordAudioHistoryHandler::~HotwordAudioHistoryHandler() {
}

bool HotwordAudioHistoryHandler::GetAudioHistoryEnabled() {
  // TODO(rlp): fill in.
  return false;
}

void HotwordAudioHistoryHandler::SetAudioHistoryEnabled(const bool enabled) {
  // TODO(rlp): fill in.
}

void HotwordAudioHistoryHandler::OnAudioHistoryEnabledChanged(
    const std::string& pref_name) {
  DCHECK(pref_name == std::string(prefs::kHotwordAudioHistoryEnabled));

  PrefService* prefs = profile_->GetPrefs();
  SetAudioHistoryEnabled(prefs->GetBoolean(prefs::kHotwordAudioHistoryEnabled));
}
