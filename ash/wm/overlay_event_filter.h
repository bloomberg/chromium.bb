// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERLAY_EVENT_FILTER_H_
#define ASH_WM_OVERLAY_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"

namespace ash {

// EventFilter for the "overlay window", which intercepts events before they are
// processed by the usual path (e.g. the partial screenshot UI, the keyboard
// overlay).  It does nothing the first time, but works when |Activate()| is
// called.  The main task of this event filter is just to stop propagation
// of any key events during activation, and also signal cancellation when keys
// for canceling are pressed.
class ASH_EXPORT OverlayEventFilter : public ui::EventHandler,
                                      public ShellObserver {
 public:
  // Windows that need to receive events from OverlayEventFilter implement this.
  class ASH_EXPORT Delegate {
   public:
    // Invoked when OverlayEventFilter needs to stop handling events.
    virtual void Cancel() = 0;

    // Returns true if the overlay should be canceled in response to |event|.
    virtual bool IsCancelingKeyEvent(ui::KeyEvent* event) = 0;

    // Returns the window that needs to receive events. NULL if no window needs
    // to receive key events from OverlayEventFilter.
    virtual aura::Window* GetWindow() = 0;
  };

  OverlayEventFilter();
  virtual ~OverlayEventFilter();

  // Starts the filtering of events.  It also notifies the specified
  // |delegate| when a key event means cancel (like Esc).  It holds the
  // pointer to the specified |delegate| until Deactivate() is called, but
  // does not take ownership.
  void Activate(Delegate* delegate);

  // Ends the filtering of events.
  void Deactivate(Delegate* delegate);

  // Cancels the partial screenshot UI.  Do nothing if it's not activated.
  void Cancel();

  // Returns true if it's currently active.
  bool IsActive();

  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(PartialScreenshotViewTest, DontStartOverOverlay);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverlayEventFilter);
};

}  // namespace ash

#endif  // ASH_WM_OVERLAY_EVENT_FILTER_H_
