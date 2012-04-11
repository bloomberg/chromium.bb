// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget.h"

class PrefService;

namespace views {
class View;
}  // namespace views

namespace chromeos {
class MessageBubble;
class MessageBubbleLinkListener;
class NetworkLibrary;

class DataPromoNotification : public views::Widget::Observer {
 public:
  DataPromoNotification();
  virtual ~DataPromoNotification();

  static void RegisterPrefs(PrefService* local_state);

  const std::string& deal_info_url() const { return deal_info_url_; }
  const std::string& deal_topup_url() const { return deal_topup_url_; }

  // Shows 3G promo notification if needed.
  void ShowOptionalMobileDataPromoNotification(
      NetworkLibrary* cros,
      views::View* host,
      MessageBubbleLinkListener* listener);

  // Closes message bubble.
  void CloseNotification();

 private:
  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // Notification bubble for 3G promo.
  MessageBubble* mobile_data_bubble_;

  // True if check for promo needs to be done,
  // otherwise just ignore it for current session.
  bool check_for_promo_;

  // Current carrier deal info URL.
  std::string deal_info_url_;

  // Current carrier deal top-up URL.
  std::string deal_topup_url_;

  // Factory for delaying showing promo notification.
  base::WeakPtrFactory<DataPromoNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataPromoNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
