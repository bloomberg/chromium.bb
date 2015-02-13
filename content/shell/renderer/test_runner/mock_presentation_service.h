// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_SERVICE_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_SERVICE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "content/shell/renderer/test_runner/mock_presentation_client.h"

namespace content {

// A mock implementation of the Mojo PresentationService to be used in tests
// with content shell. Keeps track of all MockPresentationClient instances and
// updates each instance with the new state if needed. The service is owned by
// the WebTextProxy instance (so only one exists per RenderView).
class MockPresentationService {
 public:
  MockPresentationService();
  ~MockPresentationService();

  // Imitates getting the actual screen availability for the platform.
  // If the state changed from the last call, notifies the registered clients.
  void SetScreenAvailability(bool available);

  // Resets the service state to the defaults for the next test.
  void Reset();

  // ObserverList implementation.
  void RegisterPresentationClientMock(MockPresentationClient* client);
  void UnregisterPresentationClientMock(MockPresentationClient* client);

 private:
  // The last known state of the screen availability, defaults to false.
  bool screen_availability_;
  // The registered presentation client mocks.
  ObserverList<MockPresentationClient> presentation_clients_;

  DISALLOW_COPY_AND_ASSIGN(MockPresentationService);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_PRESENTATION_SERVICE_H_
