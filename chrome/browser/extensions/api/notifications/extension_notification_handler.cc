// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"

#include "base/logging.h"

ExtensionNotificationHandler::ExtensionNotificationHandler() = default;

ExtensionNotificationHandler::~ExtensionNotificationHandler() = default;

void ExtensionNotificationHandler::OpenSettings(Profile* profile) {
  // Extension notifications don't display a settings button.
  NOTREACHED();
}
