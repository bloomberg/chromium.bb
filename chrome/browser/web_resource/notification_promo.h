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
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

class PrefService;
class Profile;

// Helper class for PromoResourceService that parses promo notification info
// from json or prefs.
class NotificationPromo
  : public base::RefCountedThreadSafe<NotificationPromo> {
 public:
  class Delegate {
   public:
     virtual ~Delegate() {}
     virtual void OnNotificationParsed(double start, double end,
                                       bool new_notification) = 0;
     // For testing.
     virtual bool IsBuildAllowed(int builds_targeted) const { return false; }
     virtual int CurrentPlatform() const { return PLATFORM_NONE; }
  };

  // Static factory for creating new notification promos.
  static NotificationPromo* Create(Profile* profile, Delegate* delegate);

  static GURL PromoServerURL();

  // Initialize from json/prefs.
  void InitFromJson(const base::DictionaryValue& json);
  void InitFromPrefs();
  // TODO(achuith): This legacy method parses json from the tip server.
  // This code will be deleted very soon. http://crbug.com/126317.
  void InitFromJsonLegacy(const base::DictionaryValue& json);

  // Can this promo be shown?
  bool CanShow() const;

  // Calculates promo notification start time with group-based time slice
  // offset.
  double StartTimeForGroup() const;

  // Helpers for NewTabPageHandler.
  void HandleClosed();
  bool HandleViewed();  // returns true if views exceeds maximum allowed.

  // Register preferences.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  friend class base::RefCountedThreadSafe<NotificationPromo>;
  NotificationPromo(Profile* profile, Delegate* delegate);
  virtual ~NotificationPromo();

  // For testing.
  friend class NotificationPromoTestDelegate;
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, GetNextQuestionValueTest);

  enum PlatformType {
    PLATFORM_NONE = 0,
    PLATFORM_WIN = 1,
    PLATFORM_MAC = 1 << 1,
    PLATFORM_LINUX = 1 << 2,
    PLATFORM_CHROMEOS = 1 << 3,
    PLATFORM_ALL = (1 << 4) -1,
  };

  // Parse the answers array element. Set the data members of this instance
  // and trigger OnNewNotification callback if necessary.
  void Parse(const base::DictionaryValue* dict);

  // Set promo notification params from a question string, which is of the form
  // <build_type>:<time_slice>:<max_group>:<max_views>:<platform>
  void ParseParams(const base::DictionaryValue* dict);

  // Check if this promo notification is new based on start/end times,
  // and trigger events accordingly.
  void CheckForNewNotification();

  // Actions on receiving a new promo notification.
  void OnNewNotification();

  // Returns an int converted from the question substring starting at index
  // till the next colon. Sets index to the location right after the colon.
  // Returns 0 if *err is true, and sets *err to true upon error.
  static int GetNextQuestionValue(const std::string& question,
                                  size_t* index,
                                  bool* err);

  // Flush data members to prefs for storage.
  void WritePrefs();

  // Tests views_ against max_views_.
  // When max_views_ is 0, we don't cap the number of views.
  bool ExceedsMaxViews() const;

  // Match our channel with specified build type.
  bool IsBuildAllowed(int builds_allowed) const;

  // Match our platform with the specified platform bitfield.
  bool IsPlatformAllowed(int target_platform) const;

  // Current platform.
  static int CurrentPlatform();

  Profile* profile_;
  Delegate* delegate_;
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

  int build_;
  int platform_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPromo);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_

