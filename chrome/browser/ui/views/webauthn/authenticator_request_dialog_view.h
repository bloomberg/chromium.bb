// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/views/window/dialog_delegate.h"

// A tab-modal dialog shown while a Web Authentication API request is active.
//
// This UI first allows the user the select the transport protocol they wish to
// use to connect their security key (either USB, BLE, NFC, or internal), and
// then guides them through the flow of setting up their security key using the
// selecting transport protocol, and finally shows success/failure indications.
class AuthenticatorRequestDialogView
    : public views::DialogDelegateView,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  AuthenticatorRequestDialogView(
      std::unique_ptr<AuthenticatorRequestDialogModel> model);
  ~AuthenticatorRequestDialogView() override;

 protected:
  void CreateContents();

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;
  void OnRequestComplete() override;

 private:
  std::unique_ptr<AuthenticatorRequestDialogModel> model_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_VIEW_H_
