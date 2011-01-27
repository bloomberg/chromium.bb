// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/notification_defines.h"

OutgoingNotificationData::OutgoingNotificationData()
    : send_content(false),
      priority(0),
      require_subscription(false),
      write_to_cache_only(false) {
}

OutgoingNotificationData::~OutgoingNotificationData() {}
