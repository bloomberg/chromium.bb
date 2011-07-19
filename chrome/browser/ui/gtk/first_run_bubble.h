// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the First Run bubble, the dialog box
// presented on first run of Chromium. There can only ever be a single
// bubble open, so the class presents only static methods.

#ifndef CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"

class Profile;

class FirstRunBubble : public BubbleDelegateGtk {
 public:
  // Shows the first run bubble, pointing at |rect|.
  static void Show(Profile* profile,
                   GtkWidget* anchor,
                   const gfx::Rect& rect,
                   FirstRun::BubbleType bubble_type);

  // Overridden from BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  FirstRunBubble(Profile* profile,
                 GtkWidget* anchor,
                 const gfx::Rect& rect,
                 FirstRun::BubbleType bubble_type);
  virtual ~FirstRunBubble();

  // Create and pack widgets for different bubble types.
  void InitializeContentForLarge(std::vector<GtkWidget*>* labels);
  void InitializeContentForOEM(std::vector<GtkWidget*>* labels);
  void InitializeContentForMinimal(std::vector<GtkWidget*>* labels);

  // Contains some common set up for the labels in the bubble. |width| is a
  // resource that holds the desired width for the labels.
  void InitializeLabels(int width_resource, std::vector<GtkWidget*>* labels);

  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleDestroy);
  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleKeepButton);
  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleChangeButton);

  // Our current profile.
  Profile* profile_;

  // Provides colors and stuff.
  GtkThemeService* theme_service_;

  // The widget we anchor to, and a descendant of the toplevel window we
  // are transient for.
  GtkWidget* anchor_;

  // We let the BubbleGtk own our content, and then we delete ourself when the
  // widget is destroyed (when the BubbleGtk is destroyed).
  GtkWidget* content_;

  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
