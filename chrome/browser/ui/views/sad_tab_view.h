// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/sad_tab.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
class TextButton;
}

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A views::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public chrome::SadTab,
                   public views::View,
                   public views::LinkListener,
                   public views::ButtonListener {
 public:
  SadTabView(content::WebContents* web_contents, chrome::SadTabKind kind);
  virtual ~SadTabView();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* source,
                             const ui::Event& event) OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  // Overridden from chrome::SadTab:
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

  views::Label* CreateLabel(const string16& text);
  views::Link* CreateLink(const string16& text);

  content::WebContents* web_contents_;
  chrome::SadTabKind kind_;
  bool painted_;
  views::Label* message_;
  views::Link* help_link_;
  views::Link* feedback_link_;
  views::TextButton* reload_button_;

  DISALLOW_COPY_AND_ASSIGN(SadTabView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H__
