// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_presentation_client.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"

namespace content {

MockPresentationClient::MockPresentationClient()
    : controller_(nullptr),
      screen_availability_(false) {
}

MockPresentationClient::~MockPresentationClient() {
}

void MockPresentationClient::SetScreenAvailability(bool available) {
  if (screen_availability_ == available)
    return;
  screen_availability_ = available;

  if (!controller_)
    return;
  if (!controller_->isAvailableChangeWatched())
    return;
  controller_->didChangeAvailability(screen_availability_);
}

void MockPresentationClient::setController(
    blink::WebPresentationController* controller) {
  DCHECK(controller_ != controller && (!controller || !controller_));
  controller_ = controller;
  if (controller_)
    updateAvailableChangeWatched(controller_->isAvailableChangeWatched());
}

void MockPresentationClient::updateAvailableChangeWatched(bool watched) {
  if (!watched)
    return;
  DCHECK(controller_);
  if (screen_availability_)
    controller_->didChangeAvailability(screen_availability_);
}

}  // namespace content
