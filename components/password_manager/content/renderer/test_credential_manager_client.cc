// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/test_credential_manager_client.h"

namespace password_manager {

TestCredentialManagerClient::TestCredentialManagerClient() {
}

TestCredentialManagerClient::~TestCredentialManagerClient() {
}

int TestCredentialManagerClient::GetRoutingID() {
  // Chosen by fair dice roll.
  return 4;
}

}  // namespace password_manager
