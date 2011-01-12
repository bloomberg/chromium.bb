// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_REGISTRY_READY_MODE_STATE_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_REGISTRY_READY_MODE_STATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome_frame/ready_mode/internal/ready_mode_state.h"

class InstallationState;
class Task;

enum ReadyModeStatus {
  READY_MODE_PERMANENTLY_DECLINED,
  READY_MODE_TEMPORARILY_DECLINED,
  READY_MODE_TEMPORARY_DECLINE_EXPIRED,
  READY_MODE_ACTIVE,
  READY_MODE_ACCEPTED
};  // enum ReadyModeStatus

// Implements ReadyModeState, reading state from the Registry and delegating to
// an elevated process launcher to effect state changes.
class RegistryReadyModeState : public ReadyModeState {
 public:
  // Receives notification when the Ready Mode state changes in response to a
  // user interaction. Does not receive notification when a temporary decline of
  // Ready Mode expires.
  class Observer {
   public:
    virtual ~Observer() {}
    // Indicates that a state change has occurred, passing the new status.
    virtual void OnStateChange(ReadyModeStatus status) = 0;
  };  // class Observer

  // Construct an instance backed by the specified key (pre-existing under
  // HKCU or HKLM). The provided duration indicates how long, after a temporary
  // decline, Ready Mode re-activates.
  //
  // Takes ownership of the Observer instance, which may be null if the client
  // does not need to be notified of changes.
  RegistryReadyModeState(const std::wstring& key_name,
                         base::TimeDelta temporary_decline_duration,
                         Observer* observer);
  virtual ~RegistryReadyModeState();

  // Returns the current Ready Mode status.
  ReadyModeStatus GetStatus();

  // Transitions from temporarily declined to active Ready Mode.
  void ExpireTemporaryDecline();

  // ReadyModeState implementation
  virtual void TemporarilyDeclineChromeFrame();
  virtual void PermanentlyDeclineChromeFrame();
  virtual void AcceptChromeFrame();

 protected:
  // allow dependency replacement via derivation for tests
  virtual base::Time GetNow();

 private:
  // Sends the result of GetStatus() to our observer.
  void NotifyObserver();
  // Retrieves state from the registry. Returns true upon success.
  bool GetValue(int64* value, bool* exists);
  // Refreshes the process state after mutating installation state.
  void RefreshStateAndNotify();

  base::TimeDelta temporary_decline_duration_;
  std::wstring key_name_;
  scoped_ptr<Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(RegistryReadyModeState);
};  // class RegistryReadyModeState

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_REGISTRY_READY_MODE_STATE_H_
