// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_presentation_client.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/mock_presentation_service.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"

namespace content {

MockPresentationClient::MockPresentationClient(MockPresentationService* service)
    : controller_(nullptr),
      screen_availability_(false),
      service_(service) {
  DCHECK(service_);
  service_->RegisterPresentationClientMock(this);
}

MockPresentationClient::~MockPresentationClient() {
  DCHECK(!controller_);
  if (service_)
    service_->UnregisterPresentationClientMock(this);
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

void MockPresentationClient::Reset() {
  screen_availability_ = false;
  service_ = nullptr;
}

void MockPresentationClient::setController(
    blink::WebPresentationController* controller) {
  DCHECK(controller_ != controller && (!controller || !controller_))
      << "Either one of the new or the old controller must be null, "
         "controller_ is " << controller_ << ", controller is " << controller;

  controller_ = controller;
}

void MockPresentationClient::updateAvailableChangeWatched(bool watched) {
  if (!watched)
    return;
  DCHECK(controller_);
  if (screen_availability_)
    controller_->didChangeAvailability(screen_availability_);
}

}  // namespace content
