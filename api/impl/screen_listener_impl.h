// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_SCREEN_LISTENER_IMPL_H_
#define API_IMPL_SCREEN_LISTENER_IMPL_H_

#include "api/impl/screen_list.h"
#include "api/public/screen_info.h"
#include "api/public/screen_listener.h"
#include "base/macros.h"

namespace openscreen {

class ScreenListenerImpl final : public ScreenListener {
 public:
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    void SetListenerImpl(ScreenListenerImpl* listener);

    virtual void StartListener() = 0;
    virtual void StartAndSuspendListener() = 0;
    virtual void StopListener() = 0;
    virtual void SuspendListener() = 0;
    virtual void ResumeListener() = 0;
    virtual void SearchNow(State from) = 0;

   protected:
    void SetState(State state) { listener_->SetState(state); }

    ScreenListenerImpl* listener_ = nullptr;
  };

  // |observer| is optional.  If it is provided, it will receive appropriate
  // notifications about this ScreenListener.  |delegate| is required and is
  // used to implement state transitions.
  ScreenListenerImpl(Observer* observer, Delegate* delegate);
  ~ScreenListenerImpl() override;

  // Called by |delegate_| when there are updates to the available screens.
  void OnScreenAdded(const ScreenInfo& info);
  void OnScreenChanged(const ScreenInfo& info);
  void OnScreenRemoved(const ScreenInfo& info);
  void OnAllScreensRemoved();

  // Called by |delegate_| when an internal error occurs.
  void OnError(ScreenListenerError error);

  // ScreenListener overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  bool SearchNow() override;

  const std::vector<ScreenInfo>& GetScreens() const override;

 private:
  // Called by |delegate_| to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // Notifies |observer_| if the transition to |state_| is one that is watched
  // by the observer interface.
  void MaybeNotifyObserver();

  Delegate* const delegate_;
  ScreenList screen_list_;

  DISALLOW_COPY_AND_ASSIGN(ScreenListenerImpl);
};

}  // namespace openscreen

#endif  // API_IMPL_SCREEN_LISTENER_IMPL_H_
