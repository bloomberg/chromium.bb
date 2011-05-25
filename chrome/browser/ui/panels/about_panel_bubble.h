// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_ABOUT_PANEL_BUBBLE_H_
#define CHROME_BROWSER_UI_PANELS_ABOUT_PANEL_BUBBLE_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "views/controls/link_listener.h"

class Browser;
class Extension;
namespace views {
class ImageView;
class Label;
class Link;
class Textfield;
}

class AboutPanelBubble : public Bubble,
                         public BubbleDelegate {
 public:
  // Returns NULL if no extension can be found for |browser|.
  static AboutPanelBubble* Show(views::Widget* parent,
                                const gfx::Rect& position_relative_to,
                                BubbleBorder::ArrowLocation arrow_location,
                                SkBitmap icon,
                                Browser* browser);

 private:
  friend class PanelBrowserViewTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, AboutPanelBubble);

  class AboutPanelBubbleView : public views::View,
                               public views::LinkListener {
   public:
    AboutPanelBubbleView(SkBitmap icon,
                         Browser* browser,
                         const Extension* extension);

   private:
    friend class PanelBrowserViewTest;
    FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, AboutPanelBubble);

    virtual ~AboutPanelBubbleView() { }

    // Overridden from View:
    virtual void Layout() OVERRIDE;
    virtual gfx::Size GetPreferredSize() OVERRIDE;

    // Overridden from LinkListener:
    virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

    views::ImageView* icon_;
    views::Label* title_;
    views::Label* install_date_;
    views::Textfield* description_;
    views::Link* uninstall_link_;
    views::Link* report_abuse_link_;

    DISALLOW_COPY_AND_ASSIGN(AboutPanelBubbleView);
  };

  AboutPanelBubble();
  virtual ~AboutPanelBubble() { }

  // Overridden from BubbleDelegate:
  virtual void BubbleClosing(Bubble* info_bubble, bool closed_by_escape)
      OVERRIDE {}
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;
  virtual std::wstring accessible_name() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AboutPanelBubble);
};

#endif  // CHROME_BROWSER_UI_PANELS_ABOUT_PANEL_BUBBLE_H_
