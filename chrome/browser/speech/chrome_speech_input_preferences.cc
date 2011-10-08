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
    : filter_profanities_(
          pref_service->GetBoolean(prefs::kSpeechInputFilterProfanities)) {
}

ChromeSpeechInputPreferences::~ChromeSpeechInputPreferences() {
}

bool ChromeSpeechInputPreferences::filter_profanities() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return filter_profanities_;
}

void ChromeSpeechInputPreferences::set_filter_profanities(
    bool filter_profanities) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeSpeechInputPreferences::set_filter_profanities,
                   this, filter_profanities));
    return;
  }
  filter_profanities_ = filter_profanities;
}
