// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel() = default;
AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed();
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  for (auto& observer : observers_)
    observer.OnRequestComplete();
}
