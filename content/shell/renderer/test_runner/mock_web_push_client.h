// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_PUSH_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_PUSH_CLIENT_H_

#include <string>

#include "third_party/WebKit/public/platform/WebPushClient.h"

namespace blink {
class WebServiceWorkerProvider;
class WebString;
}  // namespace blink

namespace content {

// MockWebPushClient is a mock implementation of WebPushClient to be able to
// test the Push Message API in Blink without depending on the content layer.
// The mock, for legacy reasons, automatically fails if it wasn't set to another
// state. Consumers can set its success values and error values by calling
// respectively SetMockSuccessValues and SetMockErrorValues. When
// SetMockSuccessValues is called, the mock will always succeed until
// SetMockErrorValues is called at which point it will always fail.
class MockWebPushClient : public blink::WebPushClient {
 public:
  MockWebPushClient();
  virtual ~MockWebPushClient();

  void SetMockSuccessValues(const std::string& end_point,
                            const std::string& registration_id);

  void SetMockErrorValues(const std::string& message);

 private:
  // WebPushClient implementation.
  virtual void registerPushMessaging(
      const blink::WebString& sender_id,
      blink::WebPushRegistrationCallbacks* callbacks);
  virtual void registerPushMessaging(
      const blink::WebString& sender_id,
      blink::WebPushRegistrationCallbacks* callbacks,
      blink::WebServiceWorkerProvider* service_worker_provider);

  std::string end_point_;
  std::string registration_id_;
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(MockWebPushClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_PUSH_CLIENT_H_
