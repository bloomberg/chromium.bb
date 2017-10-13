// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PALETTE_DELEGATE_H_
#define ASH_PALETTE_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "ui/events/devices/stylus_state.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |PaletteTray|, to perform
// Chrome-specific actions.
// TODO(jamescook): Move this to //ash/system/palette.
class PaletteDelegate {
 public:
  using EnableListener = base::Callback<void(bool)>;
  using EnableListenerSubscription =
      base::CallbackList<void(bool)>::Subscription;

  virtual ~PaletteDelegate() {}

  // Sets callback function that will receive the current state of the palette
  // enabled pref. The callback will be invoked once the initial pref value is
  // available.
  virtual std::unique_ptr<EnableListenerSubscription> AddPaletteEnableListener(
      const EnableListener& on_state_changed) = 0;

  // Returns true if the palette should be automatically opened on an eject
  // event.
  virtual bool ShouldAutoOpenPalette() = 0;

  // Returns true if the palette should be displayed. This is the one-shot
  // equivalent to AddPaletteEnableListener.
  virtual bool ShouldShowPalette() = 0;

 private:
  DISALLOW_ASSIGN(PaletteDelegate);
};

}  // namespace ash

#endif  // ASH_PALETTE_DELEGATE_H_
