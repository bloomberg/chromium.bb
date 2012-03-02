// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_GTK_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_GTK_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_view_gtk_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class ConstrainedWindowGtk;
class RenderViewContextMenuGtk;
class TabContents;
class WebDragBookmarkHandlerGtk;

// A chrome/ specific class that extends TabContentsViewGtk with features like
// constrained windows, which live in chrome/.
class ChromeWebContentsViewGtkDelegate
    : public content::WebContentsViewGtkDelegate {
 public:
  ChromeWebContentsViewGtkDelegate();
  virtual ~ChromeWebContentsViewGtkDelegate();

  // Unlike Windows, ConstrainedWindows need to collaborate with the
  // TabContentsViewGtk to position the dialogs.
  void AttachConstrainedWindow(ConstrainedWindowGtk* constrained_window);
  void RemoveConstrainedWindow(ConstrainedWindowGtk* constrained_window);

  // Overridden from WebContentsViewGtkDelegate:
  virtual void WrapView(content::TabContentsViewGtk* view) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual void OnCreateViewForWidget() OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual gboolean OnNativeViewFocusEvent(GtkWidget* widget,
                                          GtkDirectionType type,
                                          gboolean* return_value) OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

 private:
  // Sets the location of the constrained windows.
  CHROMEGTK_CALLBACK_1(ChromeWebContentsViewGtkDelegate, void,
                       OnSetFloatingPosition,
                       GtkAllocation*);

  // Contains |expanded_| as its GtkBin member.
  ui::OwnedWidgetGtk floating_;

  // Our owner. Also owns our child widgets.
  content::TabContentsViewGtk* view_;

  // The UI for the constrained dialog currently displayed. This is owned by
  // TabContents, not the view.
  ConstrainedWindowGtk* constrained_window_;

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  scoped_ptr<RenderViewContextMenuGtk> context_menu_;

  // The chrome specific delegate that receives events from WebDragDestGtk.
  scoped_ptr<WebDragBookmarkHandlerGtk> bookmark_handler_gtk_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_GTK_DELEGATE_H_
