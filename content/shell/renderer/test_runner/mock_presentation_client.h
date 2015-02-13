// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_CLIENT_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"

namespace content {

class MockPresentationService;

// A mock implementation of WebPresentationClient to be used in tests with
// content shell. One instance exists per frame, owned by the WebFrameTestProxy.
// Must be registered with MockPresentationService to get the mock calls from
// the test page.
class MockPresentationClient : public blink::WebPresentationClient {
 public:
  explicit MockPresentationClient(MockPresentationService* service);
  virtual ~MockPresentationClient();

  // Used to imitate the screen availability change by the tests.
  void SetScreenAvailability(bool available);

  // Resets the client test state to the defaults for the next test.
  void Reset();

  // blink::WebPresentationClient implementation.
  virtual void setController(blink::WebPresentationController* controller);
  virtual void updateAvailableChangeWatched(bool watched);

 private:
  // The actual WebPresentationController implementation that's being tested.
  // Associated with the same frame as this client.
  blink::WebPresentationController* controller_;

  // The last known state of the screen availability. Defaults to false.
  bool screen_availability_;

  // The common mock service for all mock clients within the same RenderView.
  // Used to register/unregister itself from.
  MockPresentationService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockPresentationClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_CLIENT_H_
