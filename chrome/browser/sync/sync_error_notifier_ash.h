// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_ASH_H_
#define CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_ASH_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/sync/sync_error_controller.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Shows sync-related errors as notifications in Ash.
class SyncErrorNotifier : public SyncErrorController::Observer,
                          public KeyedService {
 public:
  SyncErrorNotifier(SyncErrorController* controller, Profile* profile);
  virtual ~SyncErrorNotifier();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // SyncErrorController::Observer:
  virtual void OnErrorChanged() OVERRIDE;

 private:
  // The error controller to query for error details.
  SyncErrorController* error_controller_;

  // The Profile this service belongs to.
  Profile* profile_;

  // Used to keep track of the message center notification.
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifier);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_ASH_H_
