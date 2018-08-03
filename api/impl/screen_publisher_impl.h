// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_PUBLISHER_IMPL_H_
#define API_PUBLIC_SCREEN_PUBLISHER_IMPL_H_

#include "api/public/screen_publisher.h"
#include "base/macros.h"

namespace openscreen {

class ScreenPublisherImpl final : public ScreenPublisher {
 public:
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    void SetPublisherImpl(ScreenPublisherImpl* publisher);

    virtual void StartPublisher() = 0;
    virtual void StartAndSuspendPublisher() = 0;
    virtual void StopPublisher() = 0;
    virtual void SuspendPublisher() = 0;
    virtual void ResumePublisher() = 0;
    virtual void UpdateFriendlyName(const std::string& friendly_name) = 0;

   protected:
    void SetState(State state) { publisher_->SetState(state); }

    ScreenPublisherImpl* publisher_ = nullptr;
  };

  // |observer| is optional.  If it is provided, it will receive appropriate
  // notifications about this ScreenPublisher.  |delegate| is required and is
  // used to implement state transitions.
  ScreenPublisherImpl(Observer* observer, Delegate* delegate);
  ~ScreenPublisherImpl() override;

  // ScreenPublisher overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  void UpdateFriendlyName(const std::string& friendly_name) override;

 private:
  // Called by |delegate_| to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // Notifies |observer_| if the transition to |state_| is one that is watched
  // by the observer interface.
  void MaybeNotifyObserver();

  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(ScreenPublisherImpl);
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_PUBLISHER_IMPL_H_
