// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/bookmarks_handler.h"

#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"

BookmarksHandler::BookmarksHandler() {
}

BookmarksHandler::~BookmarksHandler() {}

void BookmarksHandler::RegisterMessages() {
}

void BookmarksHandler::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
}
