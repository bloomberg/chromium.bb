// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_H_
#define CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_H_

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Shows elevation needed for recovery component install on the wrench menu
// using a bubble view and a menu item.
class RecoveryInstallGlobalError : public GlobalErrorWithStandardBubble,
                                   public KeyedService {
 public:
  explicit RecoveryInstallGlobalError(Profile* profile);
  virtual ~RecoveryInstallGlobalError();

 private:
  // KeyedService:
  virtual void Shutdown() override;

  // GlobalErrorWithStandardBubble:
  virtual Severity GetSeverity() override;
  virtual bool HasMenuItem() override;
  virtual int MenuItemCommandID() override;
  virtual base::string16 MenuItemLabel() override;
  virtual int MenuItemIconResourceID() override;
  virtual void ExecuteMenuItem(Browser* browser) override;
  virtual bool HasBubbleView() override;
  virtual bool HasShownBubbleView() override;
  virtual void ShowBubbleView(Browser* browser) override;
  virtual gfx::Image GetBubbleViewIcon() override;
  virtual base::string16 GetBubbleViewTitle() override;
  virtual std::vector<base::string16> GetBubbleViewMessages() override;
  virtual base::string16 GetBubbleViewAcceptButtonLabel() override;
  virtual bool ShouldAddElevationIconToAcceptButton() override;
  virtual base::string16 GetBubbleViewCancelButtonLabel() override;
  virtual void OnBubbleViewDidClose(Browser* browser) override;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) override;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) override;

  bool HasElevationNotification() const;
  void OnElevationRequirementChanged();

  bool elevation_needed_;

  // The Profile this service belongs to.
  Profile* profile_;

  // Monitors registry change for recovery component install.
  PrefChangeRegistrar pref_registrar_;

  bool has_shown_bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(RecoveryInstallGlobalError);
};

#endif  // CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_H_
