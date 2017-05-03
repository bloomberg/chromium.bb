// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_TEST_DESTRUCTION_OBSERVABLE_H_
#define MEDIA_BASE_ANDROID_TEST_DESTRUCTION_OBSERVABLE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Utility class to help tests set expectations about the lifetime of mocks that
// they don't own.  The mocks in question must derive from
// DestructionObservable.  The test can call CreateDestructionObserver() to
// create the observer on which it may set expectations.  By default, is is okay
// but not required to destroy the observable.
class DestructionObservable {
 private:
  class DestructionObserverInterface {
   public:
    DestructionObserverInterface() = default;
    virtual ~DestructionObserverInterface() {}

    virtual void OnDestroyed() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(DestructionObserverInterface);
  };

 public:
  // Mock that calls OnDestroyed when the observable is destroyed.  This
  // will replace the destruction callback on the mock.
  class DestructionObserver : testing::NiceMock<DestructionObserverInterface> {
   protected:
    DestructionObserver();

   public:
    ~DestructionObserver();

    // Make destruction optional.  This will not fail even if destruction has
    // already occured.  Useful if you want to verify that ExpectDestruction()
    // has succeeded by the time this is called.
    void DestructionIsOptional();

    // Indicate that the object must be destroyed before the observer is, or
    // before the expectation is changed.  Normally, the only reasonable next
    // expecation is DestructionIsOptional, to verify that the codec has been
    // destroyed by a certain point in the test.
    //
    // Note that if the object has already been destroyed, then this call will
    // fail the test; either some previous expectation of ours should have
    // failed (e.g., DoNotAllowDestruction, but the object was destroyed), in
    // which case us failing is fine.  Or, if no previous expectation failed,
    // then it's likely that the test didn't set expectations properly.
    void ExpectDestruction();

    // Indicate that the observable may not be destroyed before the observer
    // is destroyed, or the expection is changed.  This will fail immediately
    // if the observable has already been destroyed.
    void DoNotAllowDestruction();

   private:
    friend class DestructionObservable;

    // Return a callback gthat will notify us about destruction.
    base::Closure GetCallback();

    // Called by the callback when the observable is destroyed.
    void OnDestroyed();

    // Called by OnDestroyed, so that we can set expectations on it.
    MOCK_METHOD0(MockOnDestroyed, void());

    // Has the observable been destroyed already?
    bool destroyed_ = false;

    base::WeakPtrFactory<DestructionObserver> weak_factory_;

    DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
  };

  DestructionObservable();
  virtual ~DestructionObservable();

  // Create a DestructionObserver for us.  There can be only one.
  std::unique_ptr<DestructionObserver> CreateDestructionObserver();

 private:
  // Callback that we'll call when we're destroyed.
  base::Closure destruction_cb_;

  DISALLOW_COPY_AND_ASSIGN(DestructionObservable);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_TEST_DESTRUCTION_OBSERVABLE_H_
