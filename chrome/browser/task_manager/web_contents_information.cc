// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/web_contents_information.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace task_manager {

WebContentsInformation::WebContentsInformation() {}

WebContentsInformation::~WebContentsInformation() {}

NotificationObservingWebContentsInformation::
    NotificationObservingWebContentsInformation() {}

NotificationObservingWebContentsInformation::
    ~NotificationObservingWebContentsInformation() {}

void NotificationObservingWebContentsInformation::StartObservingCreation(
    const NewWebContentsCallback& callback) {
  observer_callback_ = callback;
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void NotificationObservingWebContentsInformation::StopObservingCreation() {
  registrar_.Remove(
      this,
      content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  observer_callback_.Reset();
}

void NotificationObservingWebContentsInformation::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::WebContents* web_contents =
      content::Source<content::WebContents>(source).ptr();

  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      if (CheckOwnership(web_contents))
        observer_callback_.Run(web_contents);
      break;
  }
}

}  // namespace task_manager
