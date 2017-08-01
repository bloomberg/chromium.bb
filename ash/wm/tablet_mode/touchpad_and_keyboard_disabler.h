// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TOUCHPAD_AND_KEYBOARD_DISABLER_H_
#define ASH_WM_TABLET_MODE_TOUCHPAD_AND_KEYBOARD_DISABLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// TouchpadAndKeyboardDisabler is an implementation detail of
// ScopedDisableInternalMouseAndKeyboardOzone. It handles disabling touchpad
// and keyboard state. TouchpadAndKeyboardDisabler may need to outlive
// ScopedDisableInternalMouseAndKeyboardOzone, as such the destructor is
// private. Use Destroy() to signal the TouchpadAndKeyboardDisabler should be
// destroyed. Calling Destroy() deletes immediately if not awaiting an ack,
// otherwise destruction happens after the ack is received, or when the Shell is
// destroyed, whichever comes first.
class ASH_EXPORT TouchpadAndKeyboardDisabler : public ShellObserver {
 public:
  // This class is an implementation detail and exists purely for testing.
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Called from TouchpadAndKeyboardDisabler's constructor to asynchronously
    // initiate the disable. The supplied callback should be called to indicate
    // whether the disable succeeded or not.
    using DisableResultClosure = base::OnceCallback<void(bool)>;
    virtual void Disable(DisableResultClosure callback) = 0;

    // If the callback succeeds this is called to hide the cursor. If the
    // callback fails, this is not called.
    virtual void HideCursor() = 0;

    // Called if the Disable() succeeds when the state should be reenabled.
    virtual void Enable() = 0;
  };

  // Only tests need supply a non-null value.
  TouchpadAndKeyboardDisabler(std::unique_ptr<Delegate> delegate = nullptr);

  // Called to signal the TouchpadAndKeyboardDisabler is no longer needed, see
  // class description for details.
  void Destroy();

 private:
  ~TouchpadAndKeyboardDisabler() override;

  // Callback from running the disable closure.
  void OnDisableAck(bool succeeded);

  // ShellObserver:
  void OnShellDestroyed() override;

  std::unique_ptr<Delegate> delegate_;

  // Set to true when the ack from the DisableClosure is received.
  bool got_ack_ = false;

  // Set to true in OnDisableAck().
  bool did_disable_ = false;

  // Set to true in Destroy(). Indicates |this| should be deleted when
  // OnDisableAck() is called.
  bool delete_on_ack_ = false;

  base::WeakPtrFactory<TouchpadAndKeyboardDisabler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TouchpadAndKeyboardDisabler);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TOUCHPAD_AND_KEYBOARD_DISABLER_H_
