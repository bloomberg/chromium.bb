// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error_service.h"

#include <algorithm>

#include "base/stl_util.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"

GlobalErrorService::GlobalErrorService(Profile* profile) : profile_(profile) {
}

GlobalErrorService::~GlobalErrorService() {
  STLDeleteElements(&errors_);
}

void GlobalErrorService::AddGlobalError(GlobalError* error) {
  errors_.push_back(error);
  NotifyErrorsChanged(error);
}

void GlobalErrorService::RemoveGlobalError(GlobalError* error) {
  errors_.erase(std::find(errors_.begin(), errors_.end(), error));
  NotifyErrorsChanged(error);
}

GlobalError* GlobalErrorService::GetGlobalErrorByMenuItemCommandID(
    int command_id) const {
  for (std::vector<GlobalError*>::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasMenuItem() && command_id == error->MenuItemCommandID())
      return error;
  }
  return NULL;
}

int GlobalErrorService::GetFirstBadgeResourceID() const {
  for (std::vector<GlobalError*>::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasBadge())
      return error->GetBadgeResourceID();
  }
  return 0;
}

GlobalError* GlobalErrorService::GetFirstGlobalErrorWithBubbleView() const {
  for (std::vector<GlobalError*>::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasBubbleView() && !error->HasShownBubbleView())
      return error;
  }
  return NULL;
}

void GlobalErrorService::NotifyErrorsChanged(GlobalError* error) {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
      Source<Profile>(profile_),
      Details<GlobalError>(error));
}
