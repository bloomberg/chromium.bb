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
      target_(target),
      ignore_url_parameters_(false) {
}

PageLoadNotificationObserver::~PageLoadNotificationObserver() {}

bool PageLoadNotificationObserver::IsTargetLoaded() {
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source()).ptr();
  GURL current_url = controller->GetWebContents()->GetURL();
  if (ignore_url_parameters_) {
    GURL::Replacements strip_query;
    strip_query.ClearQuery();
    return current_url.ReplaceComponents(strip_query) ==
        target_.ReplaceComponents(strip_query);
  } else {
    return current_url == target_;
  }
}

}  // namespace remoting
