// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/api/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/global_error/global_error.h"

class ProfileSyncService;
class SigninManager;

// Shows sync errors on the wrench menu using a bubble view and a
// menu item.
class SyncGlobalError : public GlobalError,
                        public ProfileSyncServiceObserver {
 public:
  SyncGlobalError(ProfileSyncService* service, SigninManager* signin);
  virtual ~SyncGlobalError();

  virtual bool HasBadge() OVERRIDE;

  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;

  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

 private:
  string16 bubble_accept_label_;
  string16 bubble_message_;
  string16 menu_label_;
  ProfileSyncService* service_;
  SigninManager* signin_;

  DISALLOW_COPY_AND_ASSIGN(SyncGlobalError);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
