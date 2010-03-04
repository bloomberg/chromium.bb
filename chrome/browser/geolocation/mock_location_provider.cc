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

// Mock location provider that automatically calls back it's client when
// StartProvider is called.
class AutoMockLocationProvider : public MockLocationProvider {
 public:
  explicit AutoMockLocationProvider(bool has_valid_location)
      : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
    if (has_valid_location) {
      position_.accuracy = 3;
      position_.latitude = 4.3;
      position_.longitude = -7.8;
      position_.timestamp = 4567;
    } else {
      position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    }
  }
  virtual bool StartProvider() {
    MockLocationProvider::StartProvider();
    MessageLoop::current()->PostTask(
        FROM_HERE, task_factory_.NewRunnableMethod(
            &MockLocationProvider::UpdateListeners));
    return true;
  }

  ScopedRunnableMethodFactory<MockLocationProvider> task_factory_;
};

LocationProviderBase* NewMockLocationProvider() {
  return new MockLocationProvider;
}

LocationProviderBase* NewAutoSuccessMockLocationProvider() {
  return new AutoMockLocationProvider(true);
}

LocationProviderBase* NewAutoFailMockLocationProvider() {
  return new AutoMockLocationProvider(false);
}
