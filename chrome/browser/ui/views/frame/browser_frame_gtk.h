// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_GTK_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "views/window/window_gtk.h"

class BrowserNonClientFrameView;
class BrowserRootView;

class BrowserFrameGtk : public BrowserFrame,
                        public views::WindowGtk {
 public:
  // Normally you will create this class by calling BrowserFrame::Create.
  // Init must be called before using this class, which Create will do for you.
  BrowserFrameGtk(BrowserView* browser_view, Profile* profile);
  virtual ~BrowserFrameGtk();

  // Creates a frame view and initializes the window.  This
  // initialization function must be called after construction, it is
  // separate to avoid recursive calling of the frame from its
  // constructor.
  virtual void Init();

  // Overridden from BrowserFrame:
  virtual views::Window* GetWindow();
  virtual int GetMinimizeButtonOffset() const;
  virtual gfx::Rect GetBoundsForTabStrip(BaseTabStrip* tabstrip) const;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const;
  virtual void UpdateThrobber(bool running);
  virtual ui::ThemeProvider* GetThemeProviderForFrame() const;
  virtual bool AlwaysUseNativeFrame() const;
  virtual views::View* GetFrameView() const;
  virtual void TabStripDisplayModeChanged();

  // Overridden from views::Widget:
  virtual ui::ThemeProvider* GetThemeProvider() const;
  virtual ui::ThemeProvider* GetDefaultThemeProvider() const;
  virtual void IsActiveChanged();
  virtual void SetInitialFocus();

 protected:
  void set_browser_frame_view(BrowserNonClientFrameView* browser_frame_view) {
    browser_frame_view_ = browser_frame_view;
  }

  // Overridden from views::WidgetGtk:
  virtual views::RootView* CreateRootView();
  virtual bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator);

  // Overriden from views::WindowGtk:
  virtual gboolean OnWindowStateEvent(GtkWidget* widget,
                                      GdkEventWindowState* event);
  virtual gboolean OnConfigureEvent(GtkWidget* widget,
                                    GdkEventConfigure* event);

 protected:
  BrowserView* browser_view() const {
    return browser_view_;
  }

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  BrowserNonClientFrameView* browser_frame_view_;

  // An unowning reference to the root view associated with the window. We save
  // a copy as a BrowserRootView to avoid evil casting later, when we need to
  // call functions that only exist on BrowserRootView (versus RootView).
  BrowserRootView* root_view_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameGtk);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_GTK_H_
