// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a mock location provider and the factory functions for
// various ways of creating it.

#include "content/browser/geolocation/mock_location_provider.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/task.h"

// The provider will always be destroyed before the thread it runs on, so ref
// counting is not required.
DISABLE_RUNNABLE_METHOD_REFCOUNT(MockLocationProvider);

MockLocationProvider* MockLocationProvider::instance_ = NULL;

MockLocationProvider::MockLocationProvider(MockLocationProvider** self_ref)
    : state_(STOPPED),
      self_ref_(self_ref),
      provider_loop_(base::MessageLoopProxy::CreateForCurrentThread()) {
  CHECK(self_ref_);
  CHECK(*self_ref_ == NULL);
  *self_ref_ = this;
}

MockLocationProvider::~MockLocationProvider() {
  CHECK(*self_ref_ == this);
  *self_ref_ = NULL;
}

void MockLocationProvider::HandlePositionChanged(const Geoposition& position) {
  if (provider_loop_->BelongsToCurrentThread()) {
    // The location arbitrator unit tests rely on this method running
    // synchronously.
    position_ = position;
    UpdateListeners();
  } else {
    Task* task = NewRunnableMethod(
        this, &MockLocationProvider::HandlePositionChanged, position);
    provider_loop_->PostTask(FROM_HERE, task);
  }
}

bool MockLocationProvider::StartProvider(bool high_accuracy) {
  state_ = high_accuracy ? HIGH_ACCURACY : LOW_ACCURACY;
  return true;
}

void MockLocationProvider::StopProvider() {
  state_ = STOPPED;
}

void MockLocationProvider::GetPosition(Geoposition* position) {
  *position = position_;
}

void MockLocationProvider::OnPermissionGranted(const GURL& requesting_frame) {
  permission_granted_url_ = requesting_frame;
}

// Mock location provider that automatically calls back it's client at most
// once, when StartProvider or OnPermissionGranted is called. Use
// |requires_permission_to_start| to select which event triggers the callback.
class AutoMockLocationProvider : public MockLocationProvider {
 public:
  AutoMockLocationProvider(bool has_valid_location,
                           bool requires_permission_to_start)
      : MockLocationProvider(&instance_),
        ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
        requires_permission_to_start_(requires_permission_to_start),
        listeners_updated_(false) {
    if (has_valid_location) {
      position_.accuracy = 3;
      position_.latitude = 4.3;
      position_.longitude = -7.8;
      // Webkit compares the timestamp to wall clock time, so we need it to be
      // contemporary.
      position_.timestamp = base::Time::Now();
    } else {
      position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    }
  }
  virtual bool StartProvider(bool high_accuracy) {
    MockLocationProvider::StartProvider(high_accuracy);
    if (!requires_permission_to_start_) {
      UpdateListenersIfNeeded();
    }
    return true;
  }

  void OnPermissionGranted(const GURL& requesting_frame) {
    MockLocationProvider::OnPermissionGranted(requesting_frame);
    if (requires_permission_to_start_) {
      UpdateListenersIfNeeded();
    }
  }

  void UpdateListenersIfNeeded() {
    if (!listeners_updated_) {
      listeners_updated_ = true;
      MessageLoop::current()->PostTask(
          FROM_HERE, task_factory_.NewRunnableMethod(
          &MockLocationProvider::HandlePositionChanged, position_));
    }
  }

  ScopedRunnableMethodFactory<MockLocationProvider> task_factory_;
  const bool requires_permission_to_start_;
  bool listeners_updated_;
};

LocationProviderBase* NewMockLocationProvider() {
  return new MockLocationProvider(&MockLocationProvider::instance_);
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
