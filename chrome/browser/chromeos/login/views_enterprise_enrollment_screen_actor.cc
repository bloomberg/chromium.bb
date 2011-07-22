// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views_enterprise_enrollment_screen_actor.h"

#include "base/logging.h"

namespace chromeos {

// ViewsEnterpriseEnrollmentScreenActor, public --------------------------------

ViewsEnterpriseEnrollmentScreenActor::ViewsEnterpriseEnrollmentScreenActor(
    ViewScreenDelegate* delegate)
    : ViewScreen<EnterpriseEnrollmentView> (delegate),
      controller_(NULL) {
}

ViewsEnterpriseEnrollmentScreenActor::~ViewsEnterpriseEnrollmentScreenActor() {
}

// ViewsEnterpriseEnrollmentScreenActor, ViewScreen implementation -------------

EnterpriseEnrollmentView* ViewsEnterpriseEnrollmentScreenActor::AllocateView() {
  return new EnterpriseEnrollmentView(controller_);
}

// ViewsEnterpriseEnrollmentScreenActor,
//     EnterpriseEnrollmentScreenActor implementation --------------------------

void ViewsEnterpriseEnrollmentScreenActor::SetController(
    EnterpriseEnrollmentUI::Controller* controller) {
  controller_ = controller;
}

void ViewsEnterpriseEnrollmentScreenActor::SetEditableUser(bool editable) {
  GetUIActor()->SetEditableUser(editable);
}

void ViewsEnterpriseEnrollmentScreenActor::PrepareToShow() {
  ViewScreen<EnterpriseEnrollmentView>::PrepareToShow();
}

void ViewsEnterpriseEnrollmentScreenActor::Show() {
  ViewScreen<EnterpriseEnrollmentView>::Show();
  // Make the focus go initially to the DOMView, so that the email input field
  // receives the focus.
  view()->RequestFocus();
}

void ViewsEnterpriseEnrollmentScreenActor::Hide() {
  ViewScreen<EnterpriseEnrollmentView>::Hide();
}

void ViewsEnterpriseEnrollmentScreenActor::ShowConfirmationScreen() {
  GetUIActor()->ShowConfirmationScreen();
}

void ViewsEnterpriseEnrollmentScreenActor::ShowAuthError(
    const GoogleServiceAuthError & error) {
  GetUIActor()->ShowAuthError(error);
}

void ViewsEnterpriseEnrollmentScreenActor::ShowAccountError() {
  GetUIActor()->ShowAccountError();
}

void ViewsEnterpriseEnrollmentScreenActor::ShowFatalAuthError() {
  GetUIActor()->ShowFatalAuthError();
}

void ViewsEnterpriseEnrollmentScreenActor::ShowFatalEnrollmentError() {
  GetUIActor()->ShowFatalEnrollmentError();
}

void ViewsEnterpriseEnrollmentScreenActor::ShowNetworkEnrollmentError() {
  GetUIActor()->ShowNetworkEnrollmentError();
}

// ViewsEnterpriseEnrollmentScreenActor, public --------------------------------

EnterpriseEnrollmentScreenActor*
ViewsEnterpriseEnrollmentScreenActor::GetUIActor() {
  DCHECK(view());
  return view()->GetActor();
}

}  // namespace chromeos
