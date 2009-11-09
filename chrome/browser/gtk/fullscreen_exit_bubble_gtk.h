// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_FULLSCREEN_EXIT_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_FULLSCREEN_EXIT_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/timer.h"
#include "chrome/common/owned_widget_gtk.h"

typedef struct _GtkFloatingContainer GtkFloatingContainer;

// FullscreenExitBubbleGTK is responsible for showing a bubble atop the screen
// in fullscreen mode, telling users how to exit and providing a click target.
class FullscreenExitBubbleGtk {
 public:
  // We place the bubble in |container|.
  explicit FullscreenExitBubbleGtk(GtkFloatingContainer* container);
  ~FullscreenExitBubbleGtk();

  void InitWidgets();

 private:
  // Hide the exit bubble.
  void Hide();

  static void OnSetFloatingPosition(GtkFloatingContainer* floating_container,
                                    GtkAllocation* allocation,
                                    FullscreenExitBubbleGtk* bubble);

  static void OnLinkClicked(GtkWidget* link,
                            FullscreenExitBubbleGtk* bubble);

  // A pointer to the floating container that is our parent.
  GtkFloatingContainer* container_;

  // The top widget of the exit bubble.
  OwnedWidgetGtk alignment_;

  // The timer that does the initial hiding of the exit bubble.
  base::OneShotTimer<FullscreenExitBubbleGtk> initial_delay_;
};

#endif  // CHROME_BROWSER_GTK_FULLSCREEN_EXIT_BUBBLE_GTK_H_
