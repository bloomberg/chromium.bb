// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_HOVER_CONTROLLER_GTK_H_
#define CHROME_BROWSER_UI_GTK_HOVER_CONTROLLER_GTK_H_

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/throb_animation.h"

// This class handles the "throbbing" of a GtkChromeButton. The visual effect
// of throbbing is created by painting partially transparent hover effects. It
// only works in non-gtk theme mode. This class mainly exists to glue an
// AnimationDelegate (C++ class) to a GtkChromeButton* (GTK/c object).
class HoverControllerGtk : public gfx::AnimationDelegate {
 public:
  virtual ~HoverControllerGtk();

  GtkWidget* button() { return button_; }

  // Throb for |cycles| cycles. This will override the current remaining
  // number of cycles. Note that a "cycle" is (somewhat unintuitively) half of
  // a complete throb revolution.
  void StartThrobbing(int cycles);

  // Get the HoverControllerGtk for a given GtkChromeButton*. It is an error
  // to call this on a widget that is not a GtkChromeButton*.
  static HoverControllerGtk* GetHoverControllerGtk(GtkWidget* button);

  // Creates a GtkChromeButton and adds a HoverControllerGtk for it.
  static GtkWidget* CreateChromeButton();

  // Stop throbbing and delete |this|.
  void Destroy();

 private:
  explicit HoverControllerGtk(GtkWidget* button);

  // Overridden from gfx::AnimationDelegate.
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE;

  CHROMEGTK_CALLBACK_1(HoverControllerGtk, gboolean, OnEnter,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(HoverControllerGtk, gboolean, OnLeave,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(HoverControllerGtk, void, OnHierarchyChanged,
                       GtkWidget*);
  CHROMEGTK_CALLBACK_0(HoverControllerGtk, void, OnDestroy);

  gfx::ThrobAnimation throb_animation_;
  gfx::SlideAnimation hover_animation_;
  GtkWidget* button_;

  ui::GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(HoverControllerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_HOVER_CONTROLLER_GTK_H_
