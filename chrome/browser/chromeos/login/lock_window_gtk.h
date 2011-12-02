// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WINDOW_GTK_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WINDOW_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/lock_window.h"
#include "chrome/browser/chromeos/login/screen_locker_delegate.h"
#include "ui/views/widget/native_widget_gtk.h"

namespace chromeos {

// A ScreenLock window that covers entire screen to keep the keyboard
// focus/events inside the grab widget.
class LockWindowGtk : public views::NativeWidgetGtk,
                      public LockWindow {
 public:
  // LockWindow implementation:
  virtual void Grab(DOMView* dom_view) OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;

 protected:
  // NativeWidgetGtk overrides:
  virtual gboolean OnButtonPress(GtkWidget* widget,
                                 GdkEventButton* event) OVERRIDE;
  virtual void OnDestroy(GtkWidget* object) OVERRIDE;
  virtual void ClearNativeFocus() OVERRIDE;
  virtual void HandleGtkGrabBroke() OVERRIDE;

 private:
  friend class LockWindow;

  LockWindowGtk();
  virtual ~LockWindowGtk();

  // Initialize the lock window.
  void Init();

  // Called when the window manager is ready to handle locked state.
  void OnWindowManagerReady();

  // Called when the all inputs are grabbed.
  void OnGrabInputs();

  // Clear current GTK grab.
  void ClearGtkGrab();

  // Try to grab all inputs. It initiates another try if it fails to
  // grab and the retry count is within a limit, or fails with CHECK.
  void TryGrabAllInputs();

  // This method tries to steal pointer/keyboard grab from other
  // client by sending events that will hopefully close menus or windows
  // that have the grab.
  void TryUngrabOtherClients();

  // Event handler for client-event.
  CHROMEGTK_CALLBACK_1(LockWindowGtk, void, OnClientEvent, GdkEventClient*)

  // The screen locker window.
  views::Widget* lock_window_;

  // The widget to grab inputs on. This is initialized by Grab to be the
  // RenderHostView displaying the WebUI.
  GtkWidget* grab_widget_;

  // True if the screen locker's window has been drawn.
  bool drawn_;

  // True if both mouse input and keyboard input are grabbed.
  bool input_grabbed_;

  base::WeakPtrFactory<LockWindowGtk> weak_factory_;

  // The number times the widget tried to grab all focus.
  int grab_failure_count_;

  // Status of keyboard and mouse grab.
  GdkGrabStatus kbd_grab_status_;
  GdkGrabStatus mouse_grab_status_;

  DISALLOW_COPY_AND_ASSIGN(LockWindowGtk);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WINDOW_GTK_H_
