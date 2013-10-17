// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/remoting/page_load_notification_observer.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

namespace remoting {

PageLoadNotificationObserver::PageLoadNotificationObserver(const GURL& target)
    : WindowedNotificationObserver(
          content::NOTIFICATION_LOAD_STOP,
          base::Bind(&PageLoadNotificationObserver::IsTargetLoaded,
                     base::Unretained(this))),
      target_(target) {
}

PageLoadNotificationObserver::~PageLoadNotificationObserver() {}

bool PageLoadNotificationObserver::IsTargetLoaded() {
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source()).ptr();
  return controller->GetWebContents()->GetURL() == target_;
}

}  // namespace remoting
