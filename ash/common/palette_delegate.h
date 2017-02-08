// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_PALETTE_DELEGATE_H_
#define ASH_COMMON_PALETTE_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "ui/events/devices/stylus_state.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |PaletteTray|, to perform
// Chrome-specific actions.
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

  // Create a new note.
  virtual void CreateNote() = 0;

  // Returns true if there is a note-taking application available.
  virtual bool HasNoteApp() = 0;

  // Returns true if the palette should be automatically opened on an eject
  // event.
  virtual bool ShouldAutoOpenPalette() = 0;

  // Returns true if the palette should be displayed. This is the one-shot
  // equivalent to AddPaletteEnableListener.
  virtual bool ShouldShowPalette() = 0;

  // Take a screenshot of the entire window.
  virtual void TakeScreenshot() = 0;

  // Take a screenshot of a user-selected region. |done| is called when the
  // partial screenshot session has finished; a screenshot may or may not have
  // been taken.
  virtual void TakePartialScreenshot(const base::Closure& done) = 0;

  // Cancels any active partial screenshot session.
  virtual void CancelPartialScreenshot() = 0;

 private:
  DISALLOW_ASSIGN(PaletteDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_PALETTE_DELEGATE_H_
