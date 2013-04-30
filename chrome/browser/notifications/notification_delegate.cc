// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_delegate.h"

void NotificationDelegate::ButtonClick(int button_index) {
}

int NotificationDelegate::process_id() const {
  return -1;
}

void NotificationDelegate::ReleaseRenderViewHost() {
}
