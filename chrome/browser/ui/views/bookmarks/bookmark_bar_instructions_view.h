// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_VIEW_H_
#pragma once

#include "views/controls/link.h"
#include "views/view.h"

namespace views {
class Label;
class Link;
}

// BookmarkBarInstructionsView is a child of the bookmark bar that is visible
// when the user has no bookmarks on the bookmark bar.
// BookmarkBarInstructionsView shows a description of the bookmarks bar along
// with a link to import bookmarks. Clicking the link results in notifying the
// delegate.
class BookmarkBarInstructionsView : public views::View,
                                    public views::LinkController {
 public:
  // The delegate is notified once the user clicks on the link to import
  // bookmarks.
  class Delegate {
   public:
    virtual void ShowImportDialog() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit BookmarkBarInstructionsView(Delegate* delegate);

  // View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnThemeChanged();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual AccessibilityTypes::Role GetAccessibleRole();

  // LinkController.
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  void UpdateColors();

  Delegate* delegate_;

  views::Label* instructions_;
  views::Link* import_link_;

  // The baseline of the child views. This is -1 if none of the views support a
  // baseline.
  int baseline_;

  // Have the colors of the child views been updated? This is initially false
  // and set to true once we have a valid ThemeProvider.
  bool updated_colors_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarInstructionsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_VIEW_H_
