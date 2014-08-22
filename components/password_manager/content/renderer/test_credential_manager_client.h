// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_TEST_CREDENTIAL_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_TEST_CREDENTIAL_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/password_manager/content/renderer/credential_manager_client.h"

namespace password_manager {

class TestCredentialManagerClient : public CredentialManagerClient {
 public:
  TestCredentialManagerClient();
  virtual ~TestCredentialManagerClient();

  virtual int GetRoutingID() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCredentialManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_TEST_CREDENTIAL_MANAGER_CLIENT_H_
