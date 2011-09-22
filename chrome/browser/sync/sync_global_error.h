// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/global_error.h"

class ProfileSyncService;

// Shows sync errors on the wrench menu using a bubble view and a
// menu item.
class SyncGlobalError : public GlobalError,
                        public ProfileSyncServiceObserver {
 public:
  explicit SyncGlobalError(ProfileSyncService* service);
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
  virtual void BubbleViewDidClose() OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed() OVERRIDE;
  virtual void BubbleViewCancelButtonPressed() OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // For non-ChromeOS we customize the "Sign in to sync" wrench menu item
  // instead of adding a new wrench menu item at the bottom.
  bool HasCustomizedSyncMenuItem();

 private:
  bool has_error_;
  ProfileSyncService* service_;

  DISALLOW_COPY_AND_ASSIGN(SyncGlobalError);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
