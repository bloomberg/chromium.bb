// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_CLIENT_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"

namespace content {

// A mock implementation of WebPresentationClient to be used in tests with
// content shell.
class MockPresentationClient : public blink::WebPresentationClient {
 public:
  explicit MockPresentationClient();
  virtual ~MockPresentationClient();

  void SetScreenAvailability(bool available);

 private:
  // From blink::WebPresentationClient.
  // Passes the Blink-side delegate to the embedder.
  virtual void setController(blink::WebPresentationController*);

  // Called when the frame attaches the first event listener to or removes the
  // last event listener from the |availablechange| event.
  virtual void updateAvailableChangeWatched(bool watched);

  blink::WebPresentationController* controller_;
  bool screen_availability_;

  DISALLOW_COPY_AND_ASSIGN(MockPresentationClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_CLIENT_H_
