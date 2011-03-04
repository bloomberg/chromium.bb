// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/font.h"
#include "views/controls/link.h"
#include "views/view.h"

class SkBitmap;
class TabContents;

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A views::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public views::View,
                   public views::LinkController {
 public:
  enum Kind {
    CRASHED,  // The tab crashed.  Display the "Aw, Snap!" page.
    KILLED    // The tab was killed.  Display the killed tab page.
  };

  explicit SadTabView(TabContents* tab_contents, Kind kind);
  virtual ~SadTabView();

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void Layout();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  static void InitClass(Kind kind);

  // Assorted resources for display.
  static SkBitmap* sad_tab_bitmap_;
  static gfx::Font* title_font_;
  static gfx::Font* message_font_;
  static std::wstring title_;
  static std::wstring message_;
  static int title_width_;

  TabContents* tab_contents_;
  views::Link* learn_more_link_;

  // Regions within the display for different components, populated by
  // Layout().
  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect message_bounds_;
  gfx::Rect link_bounds_;

  Kind kind_;

  DISALLOW_COPY_AND_ASSIGN(SadTabView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H__
