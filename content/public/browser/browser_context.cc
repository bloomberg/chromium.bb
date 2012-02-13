// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

namespace content {

BrowserContext::~BrowserContext() {
  NotificationService::current()->Notify(
      content::NOTIFICATION_BROWSER_CONTEXT_DESTRUCTION,
      content::Source<BrowserContext>(this),
      content::NotificationService::NoDetails());
}

}  // namespace content
