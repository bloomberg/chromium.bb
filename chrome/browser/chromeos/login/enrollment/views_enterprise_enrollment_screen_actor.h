// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_VIEWS_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_VIEWS_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"

namespace chromeos {

// Views specific implementation of the EnterpriseEnrollmentScreenActor.
class ViewsEnterpriseEnrollmentScreenActor
    : public EnterpriseEnrollmentScreenActor,
      public ViewScreen<EnterpriseEnrollmentView> {
 public:
  explicit ViewsEnterpriseEnrollmentScreenActor(ViewScreenDelegate* delegate);
  virtual ~ViewsEnterpriseEnrollmentScreenActor();

  // Implements ViewScreen:
  virtual EnterpriseEnrollmentView* AllocateView() OVERRIDE;

  // Implements EnterpriseEnrollmentScreenActor:
  virtual void SetController(Controller* controller) OVERRIDE;
  virtual void SetEditableUser(bool editable) OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowConfirmationScreen() OVERRIDE;
  virtual void ShowAuthError(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void ShowAccountError() OVERRIDE;
  virtual void ShowFatalAuthError() OVERRIDE;
  virtual void ShowFatalEnrollmentError() OVERRIDE;
  virtual void ShowNetworkEnrollmentError() OVERRIDE;

 private:
  // Returns UI actor to be used.
  EnterpriseEnrollmentScreenActor* GetUIActor();

  Controller* controller_;

  DISALLOW_COPY_AND_ASSIGN(ViewsEnterpriseEnrollmentScreenActor);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_VIEWS_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
