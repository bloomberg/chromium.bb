// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_driver/sync_error_controller.h"

class GlobalErrorService;
class LoginUIService;
class ProfileSyncService;

// Shows sync errors on the wrench menu using a bubble view and a menu item.
class SyncGlobalError : public GlobalErrorWithStandardBubble,
                        public SyncErrorController::Observer,
                        public KeyedService {
 public:
  SyncGlobalError(GlobalErrorService* global_error_service,
                  LoginUIService* login_ui_service,
                  SyncErrorController* error_controller,
                  ProfileSyncService* profile_sync_service);
  ~SyncGlobalError() override;

  // KeyedService:
  void Shutdown() override;

  // GlobalErrorWithStandardBubble:
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  base::string16 GetBubbleViewTitle() override;
  std::vector<base::string16> GetBubbleViewMessages() override;
  base::string16 GetBubbleViewAcceptButtonLabel() override;
  base::string16 GetBubbleViewCancelButtonLabel() override;
  void OnBubbleViewDidClose(Browser* browser) override;
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;

  // SyncErrorController::Observer:
  void OnErrorChanged() override;

 private:
  base::string16 bubble_accept_label_;
  base::string16 bubble_message_;
  base::string16 menu_label_;

  GlobalErrorService* global_error_service_;

  const LoginUIService* login_ui_service_;

  // The error controller to query for error details. Owned by the
  // ProfileSyncService this SyncGlobalError depends on.
  SyncErrorController* error_controller_;

  const ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(SyncGlobalError);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_H_
