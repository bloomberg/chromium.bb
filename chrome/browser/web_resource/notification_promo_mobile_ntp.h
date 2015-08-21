// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_MOBILE_NTP_H_
#define CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_MOBILE_NTP_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/web_resource/notification_promo.h"

namespace base {
class DictionaryValue;
class ListValue;
}

// Helper class for NotificationPromo that deals with mobile_ntp promos.
class NotificationPromoMobileNtp {
 public:
  NotificationPromoMobileNtp();
  ~NotificationPromoMobileNtp();

  // Initialize from prefs/JSON.
  // Return true if the mobile NTP promotion is valid.
  bool InitFromPrefs();
  bool InitFromJson(const base::DictionaryValue& json);

  // Return true if the promo is valid and can be shown.
  bool CanShow() const;

  bool valid() const { return valid_; }
  const std::string& text() const { return text_; }
  const std::string& text_long() const { return text_long_; }
  const std::string& action_type() const { return action_type_; }
  const base::ListValue* action_args() const { return action_args_; }
  bool requires_mobile_only_sync() const { return requires_mobile_only_sync_; }
  bool requires_sync() const { return requires_sync_; }
  bool show_on_most_visited() const { return show_on_most_visited_; }
  bool show_on_open_tabs() const { return show_on_open_tabs_; }
  bool show_as_virtual_computer() const { return show_as_virtual_computer_; }
  const std::string& virtual_computer_title() const {
    return virtual_computer_title_;
  }
  const std::string& virtual_computer_lastsync() const {
    return virtual_computer_lastsync_;
  }
  const base::DictionaryValue* payload() const { return payload_; }
  const NotificationPromo& notification_promo() const {
    return notification_promo_;
  }

 private:
  // Initialize the state and validity from the low-level notification_promo_.
  bool InitFromNotificationPromo();

  // True if InitFromPrefs/JSON was called and all mandatory fields were found.
  bool valid_;
  // The short text of the promotion (e.g. for Most Visited and Open Tabs).
  std::string text_;
  // The long text of the promotion (e.g. for Open Tabs when no tabs are open).
  std::string text_long_;
  // The action that the promotion triggers (e.g. "ACTION_EMAIL").
  std::string action_type_;
  // The title of the virtual computer (e.g. when shown on Open Tabs).
  std::string virtual_computer_title_;
  // The detailed info for the virtual computer (e.g. when shown on Open Tabs).
  std::string virtual_computer_lastsync_;
  // True if the promo should be shown only if no desktop sessions were open.
  bool requires_mobile_only_sync_;
  // True if the promo should be shown only if the user is signed in to Chrome.
  bool requires_sync_;
  // True if the promo should be shown on Most Visited pane.
  bool show_on_most_visited_;
  // True if the promo should be shown on Open Tabs pane.
  bool show_on_open_tabs_;
  // True if the promo should show the virtual computer (e.g. on Open Tabs).
  bool show_as_virtual_computer_;
  // Arguments for the action (e.g. [subject, body] for "ACTION_EMAIL").
  const base::ListValue* action_args_;
  // The entire payload for the promo.
  const base::DictionaryValue* payload_;
  // The lower-level notification promo.
  NotificationPromo notification_promo_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPromoMobileNtp);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_MOBILE_NTP_H_
