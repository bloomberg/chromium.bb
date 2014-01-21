// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace {
const int kMaxTimesToShowOptInPopup = 10;
}

HotwordService::HotwordService(Profile* profile)
    : profile_(profile) {
}

HotwordService::~HotwordService() {
}


bool HotwordService::ShouldShowOptInPopup() {
  if (profile_->IsOffTheRecord())
    return false;

  // Profile is not off the record.
  if (profile_->GetPrefs()->HasPrefPath(prefs::kHotwordSearchEnabled))
    return false;  // Already opted in or opted out;

  int number_shown = profile_->GetPrefs()->GetInteger(
      prefs::kHotwordOptInPopupTimesShown);
  return number_shown < MaxNumberTimesToShowOptInPopup();
}

int HotwordService::MaxNumberTimesToShowOptInPopup() {
  return kMaxTimesToShowOptInPopup;
}

void HotwordService::ShowOptInPopup() {
  int number_shown = profile_->GetPrefs()->GetInteger(
      prefs::kHotwordOptInPopupTimesShown);
  profile_->GetPrefs()->SetInteger(prefs::kHotwordOptInPopupTimesShown,
                                   ++number_shown);
  // TODO(rlp): actually show opt in popup when linked up to extension.
}
