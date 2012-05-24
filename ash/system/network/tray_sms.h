// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_SMS_H
#define ASH_SYSTEM_NETWORK_TRAY_SMS_H
#pragma once

#include <string>

#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace internal {

class TraySms : public SystemTrayItem {
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

 protected:
  class SmsDefaultView;
  class SmsDetailedView;
  class SmsMessageView;
  class SmsNotificationView;
  class SmsObserver;

  // Gets the most recent message. Returns false if no messages or unable to
  // retrieve the numebr and text from the message.
  bool GetLatestMessage(size_t* index, std::string* number, std::string* text);

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
