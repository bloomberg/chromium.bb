// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/web_page_view.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "views/view.h"

namespace base {
class DictionaryValue;
}

namespace views {
class GridLayout;
class Label;
}

namespace chromeos {

class ScreenObserver;

// Implements the UI for the enterprise enrollment screen in OOBE.
class EnterpriseEnrollmentView : public views::View {
 public:
  explicit EnterpriseEnrollmentView(
      EnterpriseEnrollmentScreenActor::Controller* controller);
  virtual ~EnterpriseEnrollmentView();

  // Initialize view controls and layout.
  void Init();

  EnterpriseEnrollmentScreenActor* GetActor();

  // Overridden from views::View:
  virtual void RequestFocus() OVERRIDE;

 private:
  // Overriden from views::View:
  virtual void Layout() OVERRIDE;

  EnterpriseEnrollmentScreenActor::Controller* controller_;

  EnterpriseEnrollmentScreenActor* actor_;

  // Controls.
  WebPageDomView* enrollment_page_view_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_VIEW_H_
