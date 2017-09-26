// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_handler.h"

bool NotificationHandler::ShouldDisplayOnFullScreen(Profile* profile,
                                                    const std::string& origin) {
  return false;
}
