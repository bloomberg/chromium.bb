// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detection_manager.h"

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

PasswordReuseDetectionManager::PasswordReuseDetectionManager(
    PasswordManagerClient* client)
    : client_(client) {
  DCHECK(client_);
}

PasswordReuseDetectionManager::~PasswordReuseDetectionManager() {}

void PasswordReuseDetectionManager::DidNavigateMainFrame() {
  input_characters_.clear();
}

void PasswordReuseDetectionManager::OnKeyPressed(const base::string16& text) {
  // TODO(crbug.com/657041): Implement saving of |text| and asking a store about
  // password reuse.
}

}  // namespace password_manager
