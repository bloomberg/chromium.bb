// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_web_push_client.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebPushError.h"
#include "third_party/WebKit/public/platform/WebPushPermissionStatus.h"
#include "third_party/WebKit/public/platform/WebPushRegistration.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebString;

namespace content {

MockWebPushClient::MockWebPushClient()
    : error_message_(
          "Registration failed (default mock client error message)") {
}

MockWebPushClient::~MockWebPushClient() {}

void MockWebPushClient::SetMockSuccessValues(
    const std::string& end_point, const std::string& registration_id) {
  end_point_ = end_point;
  registration_id_ = registration_id;
  error_message_ = "";
}

void MockWebPushClient::SetMockErrorValues(const std::string& message) {
  end_point_ = "";
  registration_id_ = "";
  error_message_ = message;
}

void MockWebPushClient::registerPushMessaging(
    blink::WebPushRegistrationCallbacks* callbacks,
    blink::WebServiceWorkerProvider* service_worker_provider) {
  DCHECK(callbacks);

  if (!error_message_.empty()) {
    scoped_ptr<blink::WebPushError> error(
        new blink::WebPushError(blink::WebPushError::ErrorTypeAbort,
                                WebString::fromUTF8(error_message_)));
    callbacks->onError(error.release());
  } else {
    DCHECK(!end_point_.empty() && !registration_id_.empty());

    scoped_ptr<blink::WebPushRegistration> registration(
        new blink::WebPushRegistration(WebString::fromUTF8(end_point_),
                                       WebString::fromUTF8(registration_id_)));
    callbacks->onSuccess(registration.release());
  }

  delete callbacks;
}

void MockWebPushClient::getPermissionStatus(
    blink::WebPushPermissionStatusCallback* callback,
    blink::WebServiceWorkerProvider* provider) {
  blink::WebPushPermissionStatus status;
  if (error_message_.empty())
    status = blink::WebPushPermissionStatusGranted;
  else if (error_message_.compare("deny_permission") == 0)
    status = blink::WebPushPermissionStatusDenied;
  else
    status = blink::WebPushPermissionStatusDefault;

  fprintf(stderr, "ABOUT TO CALL ON SUCCESS");
  callback->onSuccess(&status);
  delete callback;
}


}  // Namespace content
