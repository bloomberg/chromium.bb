// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#pragma once

#include "ui/views/view.h"

class TabContents;

// ContentsContainer is responsible for managing the TabContents views.
// ContentsContainer has up to two children: one for the currently active
// TabContents and one for instant's TabContents.
class ContentsContainer : public views::View {
 public:
  // Internal class name
  static const char kViewClassName[];

  explicit ContentsContainer(views::View* active);
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
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

 private:
  views::View* active_;
  views::View* preview_;
  TabContents* preview_tab_contents_;

  // The margin between the top and the active view. This is used to make the
  // preview overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
