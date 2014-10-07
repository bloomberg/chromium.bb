// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/sync_error_controller.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"

class ProfileSyncService;

// Shows sync errors on the wrench menu using a bubble view and a menu item.
class SyncGlobalError : public GlobalErrorWithStandardBubble,
                        public SyncErrorController::Observer,
                        public KeyedService {
 public:
  SyncGlobalError(SyncErrorController* error_controller,
                  ProfileSyncService* profile_sync_service);
  virtual ~SyncGlobalError();

  // KeyedService:
  virtual void Shutdown() override;

  // GlobalErrorWithStandardBubble:
  virtual bool HasMenuItem() override;
  virtual int MenuItemCommandID() override;
  virtual base::string16 MenuItemLabel() override;
  virtual void ExecuteMenuItem(Browser* browser) override;
  virtual bool HasBubbleView() override;
  virtual base::string16 GetBubbleViewTitle() override;
  virtual std::vector<base::string16> GetBubbleViewMessages() override;
  virtual base::string16 GetBubbleViewAcceptButtonLabel() override;
  virtual base::string16 GetBubbleViewCancelButtonLabel() override;
  virtual void OnBubbleViewDidClose(Browser* browser) override;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) override;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) override;

  // SyncErrorController::Observer:
  virtual void OnErrorChanged() override;

 private:
  base::string16 bubble_accept_label_;
  base::string16 bubble_message_;
  base::string16 menu_label_;

  // The error controller to query for error details. Owned by the
  // ProfileSyncService this SyncGlobalError depends on.
  SyncErrorController* error_controller_;

  const ProfileSyncService* service_;

  DISALLOW_COPY_AND_ASSIGN(SyncGlobalError);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
