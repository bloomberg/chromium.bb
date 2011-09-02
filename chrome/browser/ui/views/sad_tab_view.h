// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "views/controls/link_listener.h"
#include "views/view.h"

class TabContents;

namespace gfx {
class Font;
}

namespace views {
class ImageView;
class Label;
}

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A views::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public views::View,
                   public views::LinkListener {
 public:
  // NOTE: Do not remove or reorder the elements in this enum, and only add new
  // items at the end. We depend on these specific values in a histogram.
  enum Kind {
    CRASHED = 0,  // Tab crashed.  Display the "Aw, Snap!" page.
    KILLED        // Tab killed.  Display the "He's dead, Jim!" tab page.
  };

  SadTabView(TabContents* tab_contents, Kind kind);
  virtual ~SadTabView();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  TabContents* tab_contents_;
  Kind kind_;
  bool painted_;
  views::Label* message_;
  views::Link* help_link_;
  views::Link* feedback_link_;

  DISALLOW_COPY_AND_ASSIGN(SadTabView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H__
