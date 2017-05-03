// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/test_destruction_observable.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

DestructionObservable::DestructionObservable() {}

DestructionObservable::~DestructionObservable() {
  if (!destruction_cb_.is_null())
    destruction_cb_.Run();
}

DestructionObservable::DestructionObserver::DestructionObserver()
    : weak_factory_(this) {
  // By default, destruction is okay, but not required.
  DestructionIsOptional();
}

DestructionObservable::DestructionObserver::~DestructionObserver() {}

base::Closure DestructionObservable::DestructionObserver::GetCallback() {
  return base::Bind(&DestructionObservable::DestructionObserver::OnDestroyed,
                    weak_factory_.GetWeakPtr());
}

void DestructionObservable::DestructionObserver::OnDestroyed() {
  destroyed_ = true;
  // Notify the mock.
  MockOnDestroyed();
}

void DestructionObservable::DestructionObserver::DestructionIsOptional() {
  testing::Mock::VerifyAndClearExpectations(this);
  // We're a NiceMock, so we don't need to set any expectations.
}

void DestructionObservable::DestructionObserver::ExpectDestruction() {
  // Fail if the observerable has already been destroyed.  This may seem a
  // little odd, but our semantics are "destroyed in the future".  If it's
  // already gone at this point, then it's likely that the test didn't set
  // expectations properly before, unless some previous expectation of ours
  // already failed.  (e.g., if the test previous told us DoNotAllowDestruction
  // but the object was destroyed, and we will fail anyway).
  ASSERT_TRUE(!destroyed_);
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_CALL(*this, MockOnDestroyed()).Times(1);
}

void DestructionObservable::DestructionObserver::DoNotAllowDestruction() {
  testing::Mock::VerifyAndClearExpectations(this);
  // Fail if the observerable has already been destroyed.
  ASSERT_TRUE(!destroyed_);
  EXPECT_CALL(*this, MockOnDestroyed()).Times(0);
}

std::unique_ptr<DestructionObservable::DestructionObserver>
DestructionObservable::CreateDestructionObserver() {
  DCHECK(destruction_cb_.is_null());

  std::unique_ptr<DestructionObservable::DestructionObserver> observer(
      new DestructionObserver());

  destruction_cb_ = observer->GetCallback();

  return observer;
}

}  // namespace media
