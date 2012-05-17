// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_SMS_H
#define ASH_SYSTEM_NETWORK_TRAY_SMS_H
#pragma once

#include "ash/system/tray/tray_image_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace internal {

class TraySms : public TrayImageItem {
 public:
  TraySms();
  virtual ~TraySms();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;

  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;

 protected:
  class SmsDefaultView;
  class SmsDetailedView;
  class SmsMessageView;
  class SmsNotificationView;
  class SmsObserver;

  // Called when sms messages have changed (by tray::SmsObserver).
  void Update(bool notify);

  SmsObserver* sms_observer() const { return sms_observer_.get(); }

 private:
  SmsDefaultView* default_;
  SmsDetailedView* detailed_;
  SmsNotificationView* notification_;
  scoped_ptr<SmsObserver> sms_observer_;

  DISALLOW_COPY_AND_ASSIGN(TraySms);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_SMS_H
