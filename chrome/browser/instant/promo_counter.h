// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_PROMO_COUNTER_H_
#define CHROME_BROWSER_INSTANT_PROMO_COUNTER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/time.h"

class PrefService;
class Profile;

// PromoCounter is used to track whether a promo should be shown. The promo is
// shown for a specified number of days or sessions (launches of chrome).
class PromoCounter {
 public:
  // Creates a new PromoCounter. |pref_key| is used to store prefs related to
  // the promo. |histogram_key| is the key used to store histograms related to
  // the promo. See the .cc file for the exact prefs and histogram values used.
  // |ShouldShow| returns true until the users restarts chrome |max_sessions| or
  // runs Chrome for |max_days|, or |Hide| is invoked.
  PromoCounter(Profile* profile,
               const std::string& pref_key,
               const std::string& histogram_key,
               int max_sessions,
               int max_days);
  ~PromoCounter();

  // Registers the preferences used by PromoCounter.
  static void RegisterUserPrefs(PrefService* prefs,
                                const std::string& base_key);

  // Returns true if the promo should be shown.
  bool ShouldShow(base::Time current_time);

  // Permanently hides the promo.
  void Hide();

 private:
  // Called the first time ShouldShow is invoked. Updates the necessary pref
  // state and show_.
  void Init(base::Time current_time);

  // Invoked if the max number of sessions has been encountered.
  void MaxSessionsEncountered(base::Time current_time);

  // Invoked if the max number of days has elapsed.
  void MaxTimeLapsed(base::Time current_time);

  Profile* profile_;

  // Base key all prefs are stored under.
  const std::string pref_key_;

  // Base key used for histograms.
  const std::string histogram_key_;

  // Max number of sessions/days before the promo stops.
  const int max_sessions_;
  const int max_days_;

  // Has Init been invoked?
  bool did_init_;

  // Return value from ShouldShow.
  bool show_;

  // Initial time the promo was first shown.
  base::Time initial_show_;

  DISALLOW_COPY_AND_ASSIGN(PromoCounter);
};

#endif  // CHROME_BROWSER_INSTANT_PROMO_COUNTER_H_
