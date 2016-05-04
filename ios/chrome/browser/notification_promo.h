// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NOTIFICATION_PROMO_H_
#define IOS_CHROME_BROWSER_NOTIFICATION_PROMO_H_

#include <memory>
#include <string>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios {

// Class that parses and manages promotion data from either a finch trial, json,
// or prefs.
class NotificationPromo {
 public:
  // TODO(crbug.com/608525): Remove when this code is refactored.
  enum PromoType {
    NO_PROMO,
    NTP_NOTIFICATION_PROMO,
    NTP_BUBBLE_PROMO,
    MOBILE_NTP_SYNC_PROMO,
    MOBILE_NTP_WHATS_NEW_PROMO,
  };

  explicit NotificationPromo(PrefService* local_state);
  ~NotificationPromo();

  // Initialize from finch parameters.
  void InitFromVariations();

  // Initialize from json/prefs.
  void InitFromJson(const base::DictionaryValue& json, PromoType promo_type);
  void InitFromPrefs(PromoType promo_type);

  // Can this promo be shown?
  bool CanShow() const;

  // The time when this promo can start being viewed.
  double StartTime() const;
  // The time after which this promo no longer can be viewed.
  double EndTime() const;

  // Mark the promo as closed when the user dismisses it.
  static void HandleClosed(PromoType promo_type, PrefService* local_state);
  // Mark the promo has having been viewed. This returns true if views
  // exceeds the maximum allowed.
  static bool HandleViewed(PromoType promo_type, PrefService* local_state);

  const std::string& promo_text() const { return promo_text_; }
  PromoType promo_type() const { return promo_type_; }
  const base::DictionaryValue* promo_payload() const {
    return promo_payload_.get();
  }

  // Register preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void MigrateUserPrefs(PrefService* user_prefs);

 private:
  // For testing.
  friend class NotificationPromoTest;

  // Check if this promo notification is new based on if the promo id has
  // changed.
  void CheckForNewNotification();

  // Actions on receiving a new promo notification.
  void OnNewNotification();

  // Flush data members to prefs for storage.
  void WritePrefs();

  // Tests views_ against max_views_.
  // When max_views_ is 0, we don't cap the number of views.
  bool ExceedsMaxViews() const;

  // Tests |first_view_time_| + |max_seconds_| and -now().
  // When either is 0, we don't cap the number of seconds.
  bool ExceedsMaxSeconds() const;

  // Returns whether the parameter associated with |param_name| is inside the
  // payload.
  bool IsPayloadParam(const std::string& param_name) const;

  PrefService* local_state_;

  PromoType promo_type_;
  std::string promo_text_;

  std::unique_ptr<const base::DictionaryValue> promo_payload_;

  double start_;
  double end_;
  int promo_id_;

  // When max_views_ is 0, we don't cap the number of views.
  int max_views_;

  // When max_seconds_ is 0 or not set, we don't cap the number of seconds a
  // promo can be visible.
  int max_seconds_;

  // Set when the promo is viewed for the first time.
  double first_view_time_;

  int views_;
  bool closed_;

  bool new_notification_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPromo);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_NOTIFICATION_PROMO_H_
