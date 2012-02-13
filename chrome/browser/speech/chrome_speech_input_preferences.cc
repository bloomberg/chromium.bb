// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_input_preferences.h"

#include "base/bind.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

ChromeSpeechInputPreferences::ChromeSpeechInputPreferences(
    PrefService* pref_service)
    : filter_profanities_(
          pref_service->GetBoolean(prefs::kSpeechInputFilterProfanities)) {
}

ChromeSpeechInputPreferences::~ChromeSpeechInputPreferences() {
}

bool ChromeSpeechInputPreferences::FilterProfanities() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return filter_profanities_;
}

void ChromeSpeechInputPreferences::SetFilterProfanities(
    bool filter_profanities) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeSpeechInputPreferences::SetFilterProfanities,
                   this, filter_profanities));
    return;
  }
  filter_profanities_ = filter_profanities;
}
