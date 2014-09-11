// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/ui/focus_ring_layer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
class Widget;
}

namespace chromeos {

// FocusRingController manages the focus ring around the focused view. It
// follows widget focus change and update the focus ring layer when the focused
// view of the widget changes.
class FocusRingController : public FocusRingLayerDelegate,
                            public views::WidgetObserver,
                            public views::WidgetFocusChangeListener,
                            public views::FocusChangeListener {
 public:
  FocusRingController();
  virtual ~FocusRingController();

  // Turns on/off the focus ring.
  void SetVisible(bool visible);

 private:
  // FocusRingLayerDelegate.
  virtual void OnDeviceScaleFactorChanged() OVERRIDE;

  // Sets the focused |widget|.
  void SetWidget(views::Widget* widget);

  // Updates the focus ring to the focused view of |widget_|. If |widget_| is
  // NULL or has no focused view, removes the focus ring. Otherwise, draws it.
  void UpdateFocusRing();

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetBoundsChanged(views::Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // views::WidgetFocusChangeListener overrides:
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;

  // views::FocusChangeListener overrides:
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) OVERRIDE;

  bool visible_;

  views::Widget* widget_;
  scoped_ptr<FocusRingLayer> focus_ring_layer_;

  DISALLOW_COPY_AND_ASSIGN(FocusRingController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_CONTROLLER_H_
