// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/load_complete_listener.h"

#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

LoadCompleteListener::LoadCompleteListener(Delegate* delegate)
    : delegate_(delegate) {
  registrar_ = new content::NotificationRegistrar();
  // Register for notification of when initial page load is complete to ensure
  // that we wait until start-up is complete before calling the callback.
  registrar_->Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                  content::NotificationService::AllSources());
}

LoadCompleteListener::~LoadCompleteListener() {
  if (registrar_ && registrar_->IsRegistered(this,
          content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
          content::NotificationService::AllSources())) {
    registrar_->Remove(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                       content::NotificationService::AllSources());
  }
}

void LoadCompleteListener::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      delegate_->OnLoadCompleted();
      registrar_->Remove(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                         content::NotificationService::AllSources());
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}
