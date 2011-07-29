// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/gfx/rect.h"
#include "views/controls/link_listener.h"
#include "views/view.h"

class SkBitmap;
class TabContents;

namespace gfx {
class Font;
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
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  // Assorted resources for display.
  SkBitmap* sad_tab_bitmap_;
  gfx::Font* title_font_;
  gfx::Font* message_font_;
  string16 title_;
  string16 message_;
  int title_width_;

  TabContents* tab_contents_;
  views::Link* learn_more_link_;

  // Regions within the display for different components, populated by
  // Layout().
  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect message_bounds_;
  gfx::Rect link_bounds_;

  Kind kind_;
  bool painted_;

  DISALLOW_COPY_AND_ASSIGN(SadTabView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H__
