// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_credential_manager_client.h"

#include "third_party/WebKit/public/platform/WebCredential.h"

namespace content {

MockCredentialManagerClient::MockCredentialManagerClient() {
}

MockCredentialManagerClient::~MockCredentialManagerClient() {
}

void MockCredentialManagerClient::SetResponse(
    blink::WebCredential* credential) {
  credential_.reset(credential);
}

void MockCredentialManagerClient::dispatchFailedSignIn(
    const blink::WebCredential&,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchSignedIn(
    const blink::WebCredential&,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchSignedOut(
    NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchRequest(
    bool zeroClickOnly,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  callbacks->onSuccess(credential_.get());
  delete callbacks;
}

}  // namespace content
