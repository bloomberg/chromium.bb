// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/promo_counter.h"

#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"

// Pref keys. These are relative to pref_key_.
static const char* kShowKey = ".show";
static const char* kNumSessionsKey = ".num_sessions";
static const char* kInitialTimeKey = ".initial_time";

// Values used for histograms. These are relative to histogram_key_.
static const char* kHistogramHide = ".hide";
static const char* kHistogramMaxSessions = ".max_sessions";
static const char* kHistogramMaxTime = ".max_time";

PromoCounter::PromoCounter(Profile* profile,
                           const std::string& pref_key,
                           const std::string& histogram_key,
                           int max_sessions,
                           int max_days)
    : profile_(profile),
      pref_key_(pref_key),
      histogram_key_(histogram_key),
      max_sessions_(max_sessions),
      max_days_(max_days),
      did_init_(false),
      show_(false) {
}

PromoCounter::~PromoCounter() {
}

// static
void PromoCounter::RegisterUserPrefs(PrefService* prefs,
                                     const std::string& base_key) {
  prefs->RegisterBooleanPref((base_key + kShowKey).c_str(), true);
  prefs->RegisterIntegerPref((base_key + kNumSessionsKey).c_str(), 0);
  prefs->RegisterInt64Pref((base_key + kInitialTimeKey).c_str(), 0);
}

bool PromoCounter::ShouldShow(base::Time current_time) {
  if (!did_init_) {
    did_init_ = true;
    Init(current_time);
  }

  if (show_ && (current_time - initial_show_).InDays() >= max_days_)
    MaxTimeLapsed(current_time);

  return show_;
}

void PromoCounter::Hide() {
  show_ = false;
  did_init_ = true;
  UMA_HISTOGRAM_CUSTOM_COUNTS(histogram_key_ + kHistogramHide,
                              (base::Time::Now() - initial_show_).InHours(),
                              1, max_days_ * 24, 24);
  if (profile_->GetPrefs())
    profile_->GetPrefs()->SetBoolean((pref_key_ + kShowKey).c_str(), false);
}

void PromoCounter::Init(base::Time current_time) {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs)
    return;

  show_ = prefs->GetBoolean((pref_key_ + kShowKey).c_str());
  if (!show_)
    return;

  // The user hasn't chosen to opt in or out. Only show the opt-in if it's
  // less than max_days_ since we first showed the opt-in, or the user hasn't
  // launched the profile max_sessions_ times.
  int session_count = prefs->GetInteger((pref_key_ + kNumSessionsKey).c_str());
  int64 initial_show_int =
      prefs->GetInt64((pref_key_ + kInitialTimeKey).c_str());
  initial_show_ = base::Time(base::Time::FromInternalValue(initial_show_int));
  if (initial_show_int == 0 || initial_show_ > current_time) {
    initial_show_ = base::Time::Now();
    prefs->SetInt64((pref_key_ + kInitialTimeKey).c_str(),
                    initial_show_.ToInternalValue());
  }
  if (session_count >= max_sessions_) {
    // Time check is handled in ShouldShow.
    MaxSessionsEncountered(current_time);
  } else {
    // Up the session count.
    prefs->SetInteger((pref_key_ + kNumSessionsKey).c_str(), session_count + 1);
  }
}

void PromoCounter::MaxSessionsEncountered(base::Time current_time) {
  show_ = false;
  UMA_HISTOGRAM_CUSTOM_COUNTS(histogram_key_ + kHistogramMaxSessions,
                              (current_time - initial_show_).InHours(), 1,
                              max_days_ * 24, 24);
  if (profile_->GetPrefs())
    profile_->GetPrefs()->SetBoolean((pref_key_ + kShowKey).c_str(), false);
}

void PromoCounter::MaxTimeLapsed(base::Time current_time) {
  show_ = false;
  UMA_HISTOGRAM_CUSTOM_COUNTS(histogram_key_ + kHistogramMaxTime,
                              (current_time - initial_show_).InHours(),
                              1, max_days_ * 24, 24);
  if (profile_->GetPrefs())
    profile_->GetPrefs()->SetBoolean((pref_key_ + kShowKey).c_str(), false);
}
