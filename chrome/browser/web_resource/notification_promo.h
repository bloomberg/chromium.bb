// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_
#define CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"

namespace base {
  class DictionaryValue;
}

class PrefService;

// Helper class for PromoResourceService that parses promo notification info
// from json or prefs.
class NotificationPromo {
 public:
  class Delegate {
   public:
     virtual ~Delegate() {}
     virtual void OnNewNotification(double start, double end) = 0;
     // For testing.
     virtual bool IsBuildAllowed(int builds_targeted) const { return false; }
  };

  explicit NotificationPromo(PrefService* prefs, Delegate* delegate);

  // Initialize from json/prefs.
  void InitFromJson(const base::DictionaryValue& json);
  void InitFromPrefs();

  // Can this promo be shown?
  bool CanShow() const;

  // Calculates promo notification start time with group-based time slice
  // offset.
  double StartTimeWithOffset() const;

  // Helpers for NewTabPageHandler.
  void HandleClosed();
  bool HandleViewed();  // returns true if views exceeds maximum allowed.

  // Register preferences.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // For testing.
  friend class NotificationPromoTestDelegate;
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, GetNextQuestionValueTest);
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, NewGroupTest);

  // Users are randomly assigned to one of kMaxGroupSize + 1 buckets, in order
  // to be able to roll out promos slowly, or display different promos to
  // different groups.
  static const int kMaxGroupSize = 99;

  // Parse the answers array element. Set the data members of this instance
  // and trigger OnNewNotification callback if necessary.
  void Parse(const base::DictionaryValue* dict);

  // Set promo notification params from a question string, which is of the form
  // <build_type>:<time_slice>:<max_group>:<max_views>
  void ParseParams(const base::DictionaryValue* dict);

  // Check if this promo notification is new based on start/end times,
  // and trigger events accordingly.
  void CheckForNewNotification();

  // Actions on receiving a new promo notification.
  void OnNewNotification();

  // Create a new promo notification group.
  static int NewGroup();

  // Returns an int converted from the question substring starting at index
  // till the next colon. Sets index to the location right after the colon.
  // Returns 0 if *err is true, and sets *err to true upon error.
  static int GetNextQuestionValue(const std::string& question,
                                  size_t* index,
                                  bool* err);

  // Flush data members to prefs for storage.
  void WritePrefs();

  // Match our channel with specified build type.
  bool IsBuildAllowed(int builds_allowed) const;

  // For testing.
  bool operator==(const NotificationPromo& other) const;

  PrefService* prefs_;
  Delegate* delegate_;

  double start_;
  double end_;

  int build_;
  int time_slice_;
  int max_group_;
  int max_views_;

  int group_;
  int views_;
  std::string text_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPromo);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_

