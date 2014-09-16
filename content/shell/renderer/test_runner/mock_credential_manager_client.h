// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_CREDENTIAL_MANAGER_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_CREDENTIAL_MANAGER_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace blink {
class WebCredential;
class WebFrame;
class WebURL;
}

namespace content {

class MockCredentialManagerClient : public blink::WebCredentialManagerClient {
 public:
  MockCredentialManagerClient();
  virtual ~MockCredentialManagerClient();

  // We take ownership of the |credential|.
  void SetResponse(blink::WebCredential* credential);

  // blink::WebCredentialManager:
  virtual void dispatchFailedSignIn(const blink::WebCredential& credential,
                                    NotificationCallbacks* callbacks);
  virtual void dispatchSignedIn(const blink::WebCredential& credential,
                                NotificationCallbacks* callbacks);
  virtual void dispatchSignedOut(NotificationCallbacks* callbacks);
  virtual void dispatchRequest(
      bool zero_click_only,
      const blink::WebVector<blink::WebURL>& federations,
      RequestCallbacks* callbacks);

 private:
  scoped_ptr<blink::WebCredential> credential_;

  DISALLOW_COPY_AND_ASSIGN(MockCredentialManagerClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_CREDENTIAL_MANAGER_CLIENT_H_
