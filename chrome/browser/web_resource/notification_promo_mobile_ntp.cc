// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo_mobile_ntp.h"

#include "base/values.h"
#include "chrome/browser/web_resource/notification_promo.h"

NotificationPromoMobileNtp::NotificationPromoMobileNtp()
    : valid_(false),
      action_args_(NULL),
      payload_(NULL) {
}

NotificationPromoMobileNtp::~NotificationPromoMobileNtp() {
}

bool NotificationPromoMobileNtp::InitFromPrefs() {
  notification_promo_.InitFromPrefs(NotificationPromo::MOBILE_NTP_SYNC_PROMO);
  return InitFromNotificationPromo();
}

bool NotificationPromoMobileNtp::InitFromJson(
    const base::DictionaryValue& json) {
  notification_promo_.InitFromJson(
      json, NotificationPromo::MOBILE_NTP_SYNC_PROMO);
  return InitFromNotificationPromo();
}

bool NotificationPromoMobileNtp::CanShow() const {
  return valid() && notification_promo_.CanShow();
}

bool NotificationPromoMobileNtp::InitFromNotificationPromo() {
  valid_ = false;
  requires_mobile_only_sync_ = true;
  requires_sync_ = true;
  show_on_most_visited_ = false;
  show_on_open_tabs_ = true;
  show_as_virtual_computer_ = true;
  action_args_ = NULL;

  // These fields are mandatory and must be specified in the promo.
  payload_ = notification_promo_.promo_payload();
  if (!payload_ ||
      !payload_->GetString("promo_message_short", &text_) ||
      !payload_->GetString("promo_message_long", &text_long_) ||
      !payload_->GetString("promo_action_type", &action_type_) ||
      !payload_->GetList("promo_action_args", &action_args_) ||
      !action_args_) {
    return false;
  }

  // The rest of the fields are optional.
  valid_ = true;
  payload_->GetBoolean("promo_requires_mobile_only_sync",
                       &requires_mobile_only_sync_);
  payload_->GetBoolean("promo_requires_sync", &requires_sync_);
  payload_->GetBoolean("promo_show_on_most_visited", &show_on_most_visited_);
  payload_->GetBoolean("promo_show_on_open_tabs", &show_on_open_tabs_);
  payload_->GetBoolean("promo_show_as_virtual_computer",
                       &show_as_virtual_computer_);
  payload_->GetString("promo_virtual_computer_title", &virtual_computer_title_);
  payload_->GetString("promo_virtual_computer_lastsync",
                      &virtual_computer_lastsync_);

  return valid_;
}
