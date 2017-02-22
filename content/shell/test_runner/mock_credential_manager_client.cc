// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_credential_manager_client.h"

#include <memory>
#include <utility>

#include "third_party/WebKit/public/platform/WebCredential.h"

namespace test_runner {

MockCredentialManagerClient::MockCredentialManagerClient()
    : error_(blink::WebCredentialManagerNoError) {}

MockCredentialManagerClient::~MockCredentialManagerClient() {}

void MockCredentialManagerClient::SetResponse(
    blink::WebCredential* credential) {
  credential_.reset(credential);
}

void MockCredentialManagerClient::SetError(const std::string& error) {
  if (error == "pending")
    error_ = blink::WebCredentialManagerPendingRequestError;
  if (error == "disabled")
    error_ = blink::WebCredentialManagerDisabledError;
  if (error == "unknown")
    error_ = blink::WebCredentialManagerUnknownError;
  if (error.empty())
    error_ = blink::WebCredentialManagerNoError;
}

void MockCredentialManagerClient::dispatchStore(
    const blink::WebCredential&,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchRequireUserMediation(
    NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchGet(
    bool zero_click_only,
    bool include_passwords,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  if (error_ != blink::WebCredentialManagerNoError)
    callbacks->onError(error_);
  else
    callbacks->onSuccess(std::move(credential_));
  delete callbacks;
}

}  // namespace test_runner
