// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_CREDENTIAL_MANAGER_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_MOCK_CREDENTIAL_MANAGER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace blink {
class WebCredential;
class WebFrame;
class WebURL;
}

namespace test_runner {

class MockCredentialManagerClient : public blink::WebCredentialManagerClient {
 public:
  MockCredentialManagerClient();
  virtual ~MockCredentialManagerClient();

  // We take ownership of the |credential|.
  void SetResponse(blink::WebCredential* credential);
  void SetError(const std::string& error);

  // blink::WebCredentialManager:
  void dispatchStore(const blink::WebCredential& credential,
                     NotificationCallbacks* callbacks) override;
  void dispatchRequireUserMediation(NotificationCallbacks* callbacks) override;
  void dispatchGet(bool zero_click_only,
                   bool include_passwords,
                   const blink::WebVector<blink::WebURL>& federations,
                   RequestCallbacks* callbacks) override;

 private:
  std::unique_ptr<blink::WebCredential> credential_;
  blink::WebCredentialManagerError error_;

  DISALLOW_COPY_AND_ASSIGN(MockCredentialManagerClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_CREDENTIAL_MANAGER_CLIENT_H_
