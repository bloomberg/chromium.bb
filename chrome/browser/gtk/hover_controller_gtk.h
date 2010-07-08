// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_HOVER_CONTROLLER_GTK_H_
#define CHROME_BROWSER_GTK_HOVER_CONTROLLER_GTK_H_

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "app/gtk_signal_registrar.h"
#include "app/slide_animation.h"
#include "app/throb_animation.h"
#include "base/scoped_ptr.h"

// This class handles the "throbbing" of a GtkChromeButton. The visual effect
// of throbbing is created by painting partially transparent hover effects. It
// only works in non-gtk theme mode. This class mainly exists to glue an
// AnimationDelegate (C++ class) to a GtkChromeButton* (GTK/c object).
class HoverControllerGtk : public AnimationDelegate {
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

  // Overridden from AnimationDelegate.
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

  CHROMEGTK_CALLBACK_1(HoverControllerGtk, gboolean, OnEnter,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(HoverControllerGtk, gboolean, OnLeave,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_0(HoverControllerGtk, void, OnDestroy);

  ThrobAnimation throb_animation_;
  SlideAnimation hover_animation_;
  GtkWidget* button_;

  GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(HoverControllerGtk);
};

#endif  // CHROME_BROWSER_GTK_HOVER_CONTROLLER_GTK_H_
