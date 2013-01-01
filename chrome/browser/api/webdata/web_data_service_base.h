// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_
#define CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_

#include "content/public/browser/notification_source.h"

// Base for WebDataService class hierarchy.
class WebDataServiceBase {
 public:
  // All requests return an opaque handle of the following type.
  typedef int Handle;

  virtual ~WebDataServiceBase() {}

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  virtual void CancelRequest(Handle h) = 0;

  // Returns the notification source for this service. This may use a
  // pointer other than this object's |this| pointer.
  virtual content::NotificationSource GetNotificationSource() = 0;
};

#endif  // CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_
