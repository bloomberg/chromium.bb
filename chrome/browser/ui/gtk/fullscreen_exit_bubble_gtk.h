// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_FULLSCREEN_EXIT_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_FULLSCREEN_EXIT_BUBBLE_GTK_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"

typedef struct _GtkFloatingContainer GtkFloatingContainer;
typedef struct _GtkWidget GtkWidget;

// FullscreenExitBubbleGTK is responsible for showing a bubble atop the screen
// in fullscreen mode, telling users how to exit and providing a click target.
class FullscreenExitBubbleGtk {
 public:
  // We place the bubble in |container|.
  explicit FullscreenExitBubbleGtk(GtkFloatingContainer* container);
  virtual ~FullscreenExitBubbleGtk();

  void InitWidgets();

 private:
  GtkWidget* widget() const {
    return slide_widget_->widget();
  }

  // Hide the exit bubble.
  void Hide();

  CHROMEGTK_CALLBACK_1(FullscreenExitBubbleGtk, void, OnSetFloatingPosition,
                       GtkAllocation*);
  CHROMEGTK_CALLBACK_0(FullscreenExitBubbleGtk, void, OnLinkClicked);

  // A pointer to the floating container that is our parent.
  GtkFloatingContainer* container_;

  // The widget that animates the slide-out of fullscreen exit bubble.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // The timer that does the initial hiding of the exit bubble.
  base::OneShotTimer<FullscreenExitBubbleGtk> initial_delay_;

  ui::GtkSignalRegistrar signals_;
};

#endif  // CHROME_BROWSER_UI_GTK_FULLSCREEN_EXIT_BUBBLE_GTK_H_
