// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_
#define CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

class PrefService;
class Profile;

// Helper class for PromoResourceService that parses promo notification info
// from json or prefs.
class NotificationPromo {
 public:
  static GURL PromoServerURL();

  explicit NotificationPromo(Profile* profile);
  ~NotificationPromo();

  // Initialize from json/prefs.
  void InitFromJson(const base::DictionaryValue& json);
  void InitFromPrefs();

  // Can this promo be shown?
  bool CanShow() const;

  // Calculates promo notification start time with group-based time slice
  // offset.
  double StartTimeForGroup() const;
  double EndTime() const;

  // Helpers for NewTabPageHandler.
  void HandleClosed();
  bool HandleViewed();  // returns true if views exceeds maximum allowed.

  bool new_notification() const { return new_notification_; }

  // Register preferences.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // For testing.
  friend class NotificationPromoTest;

  // Check if this promo notification is new based on start/end times,
  // and trigger events accordingly.
  void CheckForNewNotification();

  // Actions on receiving a new promo notification.
  void OnNewNotification();

  // Flush data members to prefs for storage.
  void WritePrefs();

  // Tests views_ against max_views_.
  // When max_views_ is 0, we don't cap the number of views.
  bool ExceedsMaxViews() const;

  // True if this promo is not targeted to G+ users, or if this is a G+ user.
  bool IsGPlusRequired() const;

  Profile* profile_;
  PrefService* prefs_;

  std::string promo_text_;

  double start_;
  double end_;

  int num_groups_;
  int initial_segment_;
  int increment_;
  int time_slice_;
  int max_group_;

  // When max_views_ is 0, we don't cap the number of views.
  int max_views_;

  int group_;
  int views_;
  bool closed_;

  bool gplus_required_;

  bool new_notification_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPromo);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_
