// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/native_notification_delegate.h"

std::string NativeNotificationDelegate::id() const {
  return notification_id_;
}
