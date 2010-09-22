// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#pragma once

#include "views/view.h"

class BrowserView;
class TabContents;

namespace views {
class Widget;
}

// ContentsContainer is responsible for managing the TabContents views.
// ContentsContainer has up to two children: one for the currently active
// TabContents and one for the match preview TabContents.
class ContentsContainer : public views::View {
 public:
  ContentsContainer(BrowserView* browser_view, views::View* active);
  virtual ~ContentsContainer();

  // Makes the preview view the active view and nulls out the old active view.
  // It's assumed the caller will delete or remove the old active view
  // separately.
  void MakePreviewContentsActiveContents();

  // Sets the preview view. This does not delete the old.
  void SetPreview(views::View* preview, TabContents* preview_tab_contents);

  TabContents* preview_tab_contents() const { return preview_tab_contents_; }

  // Sets the active top margin.
  void SetActiveTopMargin(int margin);

  // Returns the bounds of the preview. If the preview isn't active this
  // retuns the bounds the preview would be shown at.
  gfx::Rect GetPreviewBounds();

  // View overrides:
  virtual void Layout();

 private:
#if defined(OS_WIN)
  class TearWindow;
#else
  typedef views::Widget TearWindow;
#endif

  // Creates and configures the tear window.
  void CreateTearWindow();

  // Creates and returns a new TearWindow.
  TearWindow* CreateTearWindowImpl();

  // Resets the bounds of the tear window.
  void PositionTearWindow();

  // Closes and deletes the tear window.
  void DeleteTearWindow();

  // Invoked when the tear window is destroyed.
  void TearWindowDestroyed();

  BrowserView* browser_view_;

  views::View* active_;

  views::View* preview_;

  TabContents* preview_tab_contents_;

  // Window used to show the page tear.
  TearWindow* tear_window_;

  // The margin between the top and the active view. This is used to make the
  // preview overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_CONTENTS_CONTAINER_H_
