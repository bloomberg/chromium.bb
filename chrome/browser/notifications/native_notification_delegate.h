// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DELEGATE_H_

#include "chrome/browser/notifications/notification_delegate.h"

// Temporary native notification delegate, it is only used
// to extract the delegate id.
// This is an interim state until the work in
// https://bugs.chromium.org/p/chromium/issues/detail?id=720345
// is completed. A small delegate shim is needed since the
// Notification object has a non virtual method (delegate_id) that is
// used all over the place whose implementation returns delegate->id()
class NativeNotificationDelegate : public NotificationDelegate {
 public:
  explicit NativeNotificationDelegate(const std::string& notification_id)
      : notification_id_(notification_id) {}
  std::string id() const override;

 private:
  ~NativeNotificationDelegate() override = default;
  const std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(NativeNotificationDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DELEGATE_H_
