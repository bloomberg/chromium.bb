// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

enum ReadyModeStatus;

class InstallationState;
class Task;

// Implements ReadyModeState, storing state in the Registry and delegating to an
// instance of InstallationState to interact with the installer. Notifies a
// single Observer when the state changes.
class RegistryReadyModeState : public ReadyModeState {
 public:
  // Receives notification when the Ready Mode state changes in response to a
  // user interaction. Does not receive notification when a temporary decline of
  // Ready Mode expires.
  class Observer {
   public:
    virtual ~Observer() {}
    // Indicates that a state change has occurred.
    virtual void OnStateChange() = 0;
  };  // class Observer

  // Construct an instance backed by the specified key
  // (pre-existing under HKCU). The provided duration indicates how long, after
  // a temporary decline, Ready Mode re-activates.
  //
  // Takes ownership of the Observer and InstallationState instances.
  RegistryReadyModeState(const std::wstring& key_name,
                         base::TimeDelta temporary_decline_duration,
                         InstallationState* installation_state,
                         Observer* observer);
  virtual ~RegistryReadyModeState();

  // Returns the current Ready Mode status, as determined using our registry
  // state and InstallationState instance.
  ReadyModeStatus GetStatus();

  // ReadyModeState implementation
  virtual void TemporarilyDeclineChromeFrame();
  virtual void PermanentlyDeclineChromeFrame();
  virtual void AcceptChromeFrame();

 protected:
  // allow dependency replacement via derivation for tests
  virtual base::Time GetNow();

 private:
  // Retrieves state from the registry. Returns true upon success.
  bool GetValue(int64* value, bool* exists);
  // Stores value in the registry. Returns true upon success.
  bool StoreValue(int64 value);

  base::TimeDelta temporary_decline_duration_;
  int temporary_decline_length_seconds_;
  std::wstring key_name_;
  scoped_ptr<InstallationState> installation_state_;
  scoped_ptr<Observer> observer_;
};  // class RegistryReadyModeState

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_REGISTRY_READY_MODE_STATE_H_
