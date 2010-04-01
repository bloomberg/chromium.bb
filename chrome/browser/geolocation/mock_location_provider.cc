// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a mock location provider and the factory functions for
// various ways of creating it.

#include "chrome/browser/geolocation/mock_location_provider.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"

MockLocationProvider* MockLocationProvider::instance_ = NULL;

MockLocationProvider::MockLocationProvider()
    : started_count_(0) {
  CHECK(instance_ == NULL);
  instance_ = this;
}

MockLocationProvider::~MockLocationProvider() {
  CHECK(instance_ == this);
  instance_ = NULL;
}

bool MockLocationProvider::StartProvider() {
  ++started_count_;
  return true;
}

void MockLocationProvider::GetPosition(Geoposition* position) {
  *position = position_;
}

void MockLocationProvider::OnPermissionGranted(const GURL& requesting_frame) {
  permission_granted_url_ = requesting_frame;
}

// Mock location provider that automatically calls back it's client when
// StartProvider is called.
class AutoMockLocationProvider : public MockLocationProvider {
 public:
  AutoMockLocationProvider(bool has_valid_location,
                           bool requires_permission_to_start)
      : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
        requires_permission_to_start_(requires_permission_to_start) {
    if (has_valid_location) {
      position_.accuracy = 3;
      position_.latitude = 4.3;
      position_.longitude = -7.8;
      position_.timestamp = base::Time::FromDoubleT(4567.8);
    } else {
      position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    }
  }
  virtual bool StartProvider() {
    MockLocationProvider::StartProvider();
    if (!requires_permission_to_start_) {
      MessageLoop::current()->PostTask(
          FROM_HERE, task_factory_.NewRunnableMethod(
              &MockLocationProvider::UpdateListeners));
    }
    return true;
  }

  void OnPermissionGranted(const GURL& requesting_frame) {
    MockLocationProvider::OnPermissionGranted(requesting_frame);
    if (requires_permission_to_start_) {
      MessageLoop::current()->PostTask(
          FROM_HERE, task_factory_.NewRunnableMethod(
              &MockLocationProvider::UpdateListeners));
    }
  }

  ScopedRunnableMethodFactory<MockLocationProvider> task_factory_;
  const bool requires_permission_to_start_;
};

LocationProviderBase* NewMockLocationProvider() {
  return new MockLocationProvider;
}

LocationProviderBase* NewAutoSuccessMockLocationProvider() {
  return new AutoMockLocationProvider(true, false);
}

LocationProviderBase* NewAutoFailMockLocationProvider() {
  return new AutoMockLocationProvider(false, false);
}

LocationProviderBase* NewAutoSuccessMockNetworkLocationProvider() {
  return new AutoMockLocationProvider(true, true);
}
