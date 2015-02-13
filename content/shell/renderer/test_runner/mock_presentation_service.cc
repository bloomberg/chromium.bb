// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_presentation_service.h"

#include "base/logging.h"

namespace content {

MockPresentationService::MockPresentationService()
    : screen_availability_(false) {
}

MockPresentationService::~MockPresentationService() {
  Reset();
}

void MockPresentationService::SetScreenAvailability(bool available) {
  if (screen_availability_ == available)
    return;
  screen_availability_ = available;

  FOR_EACH_OBSERVER(
      MockPresentationClient,
      presentation_clients_,
      SetScreenAvailability(available));
}

void MockPresentationService::Reset() {
  FOR_EACH_OBSERVER(MockPresentationClient, presentation_clients_, Reset());
  presentation_clients_.Clear();
  screen_availability_ = false;
}

void MockPresentationService::RegisterPresentationClientMock(
    MockPresentationClient* client) {
  DCHECK(client);
  presentation_clients_.AddObserver(client);
}

void MockPresentationService::UnregisterPresentationClientMock(
    MockPresentationClient* client) {
  DCHECK(client);
  presentation_clients_.RemoveObserver(client);
}

}  // namespace content
