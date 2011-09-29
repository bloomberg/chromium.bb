// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_input_preferences.h"

#include "base/bind.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"

ChromeSpeechInputPreferences::ChromeSpeechInputPreferences(
    PrefService* pref_service)
    : censor_results_(
          pref_service->GetBoolean(prefs::kSpeechInputCensorResults)) {
}

ChromeSpeechInputPreferences::~ChromeSpeechInputPreferences() {
}

bool ChromeSpeechInputPreferences::censor_results() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return censor_results_;
}

void ChromeSpeechInputPreferences::set_censor_results(bool censor_results) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeSpeechInputPreferences::set_censor_results,
                   this, censor_results));
    return;
  }
  censor_results_ = censor_results;
}
