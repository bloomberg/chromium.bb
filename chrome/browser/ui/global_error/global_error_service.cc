// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service.h"

#include <algorithm>

#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

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
  GlobalErrorBubbleViewBase* bubble = error->GetBubbleView();
  if (bubble)
    bubble->CloseBubbleView();
  NotifyErrorsChanged(error);
}

GlobalError* GlobalErrorService::GetGlobalErrorByMenuItemCommandID(
    int command_id) const {
  for (GlobalErrorList::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasMenuItem() && command_id == error->MenuItemCommandID())
      return error;
  }
  return NULL;
}

GlobalError*
GlobalErrorService::GetHighestSeverityGlobalErrorWithWrenchMenuItem() const {
  GlobalError::Severity highest_severity = GlobalError::SEVERITY_LOW;
  GlobalError* highest_severity_error = NULL;

  for (GlobalErrorList::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasMenuItem()) {
      if (!highest_severity_error || error->GetSeverity() > highest_severity) {
        highest_severity = error->GetSeverity();
        highest_severity_error = error;
      }
    }
  }

  return highest_severity_error;
}

GlobalError* GlobalErrorService::GetFirstGlobalErrorWithBubbleView() const {
  for (GlobalErrorList::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasBubbleView() && !error->HasShownBubbleView())
      return error;
  }
  return NULL;
}

void GlobalErrorService::NotifyErrorsChanged(GlobalError* error) {
  // GlobalErrorService is bound only to original profile so we need to send
  // notifications to both it and its off-the-record profile to update
  // incognito windows as well.
  std::vector<Profile*> profiles_to_notify;
  if (profile_ != NULL) {
    profiles_to_notify.push_back(profile_);
    if (profile_->IsOffTheRecord())
      profiles_to_notify.push_back(profile_->GetOriginalProfile());
    else if (profile_->HasOffTheRecordProfile())
      profiles_to_notify.push_back(profile_->GetOffTheRecordProfile());
    for (size_t i = 0; i < profiles_to_notify.size(); ++i) {
      content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
        content::Source<Profile>(profiles_to_notify[i]),
        content::Details<GlobalError>(error));
    }
  }
}
