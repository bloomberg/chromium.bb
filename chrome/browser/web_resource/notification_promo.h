// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace base {
  class DictionaryValue;
}

namespace net {
  class URLRequestContextGetter;
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

  // Initialize from json/prefs.
  void InitFromJson(const base::DictionaryValue& json, bool do_cookie_check);
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
  friend class base::RefCountedThreadSafe<NotificationPromo>;
  NotificationPromo(Profile* profile, Delegate* delegate);
  virtual ~NotificationPromo();

  // For testing.
  friend class NotificationPromoTestDelegate;
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, GetNextQuestionValueTest);
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, NewGroupTest);

  enum PlatformType {
    PLATFORM_NONE = 0,
    PLATFORM_WIN = 1,
    PLATFORM_MAC = 1 << 1,
    PLATFORM_LINUX = 1 << 2,
    PLATFORM_CHROMEOS = 1 << 3,
    PLATFORM_ALL = (1 << 4) -1,
  };

  // Flags for feature_mask_.
  enum Feature {
    NO_FEATURE = 0,
    FEATURE_GPLUS = 1,
  };

  // Users are randomly assigned to one of kMaxGroupSize + 1 buckets, in order
  // to be able to roll out promos slowly, or display different promos to
  // different groups.
  static const int kMaxGroupSize = 99;

  // Parse the answers array element. Set the data members of this instance
  // and trigger OnNewNotification callback if necessary.
  void Parse(const base::DictionaryValue* dict);

  // Set promo notification params from a question string, which is of the form
  // <build_type>:<time_slice>:<max_group>:<max_views>:<platform>:<feature_mask>
  void ParseParams(const base::DictionaryValue* dict);

  // Check if this promo notification is new based on start/end times,
  // and trigger events accordingly.
  void CheckForNewNotification(bool found_cookie);

  // Actions on receiving a new promo notification.
  void OnNewNotification();

  // Async method to get cookies from GPlus url. Used to check if user is
  // logged in to GPlus.
  void GetCookies(scoped_refptr<net::URLRequestContextGetter> getter);

  // Callback for GetCookies.
  void GetCookiesCallback(const std::string& cookies);

  // Parse cookies in search of a SID= value.
  static bool CheckForGPlusCookie(const std::string& cookies);

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

  // Match our platform with the specified platform bitfield.
  bool IsPlatformAllowed(int target_platform) const;

  // Current platform.
  static int CurrentPlatform();

  // For testing.
  bool operator==(const NotificationPromo& other) const;

  Profile* profile_;
  Delegate* delegate_;
  PrefService* prefs_;

  double start_;
  double end_;

  int build_;
  int time_slice_;
  int max_group_;
  int max_views_;
  int platform_;
  int feature_mask_;

  int group_;
  int views_;
  std::string text_;
  bool closed_;
  bool gplus_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPromo);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_H_

