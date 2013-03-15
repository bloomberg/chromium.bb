// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_GTK_H_
#define CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_GTK_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/scoped_gobject.h"

class RenderViewContextMenuGtk;
class WebDragBookmarkHandlerGtk;

namespace content {
class WebContents;
}

// A chrome/ specific class that extends WebContentsViewGtk with features like
// web contents modal dialogs, which live in chrome/.
class ChromeWebContentsViewDelegateGtk
    : public content::WebContentsViewDelegate {
 public:
  explicit ChromeWebContentsViewDelegateGtk(content::WebContents* web_contents);
  virtual ~ChromeWebContentsViewDelegateGtk();

  static ChromeWebContentsViewDelegateGtk* GetFor(
      content::WebContents* web_contents);

  GtkWidget* expanded_container() { return expanded_container_; }
  ui::FocusStoreGtk* focus_store() { return focus_store_; }

  // Unlike Windows, web contents modal dialogs need to collaborate with the
  // WebContentsViewGtk to position the dialogs.
  void AttachWebContentsModalDialog(GtkWidget* web_contents_modal_dialog);
  void RemoveWebContentsModalDialog(GtkWidget* web_contents_modal_dialog);

  // Overridden from WebContentsViewDelegate:
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params,
      content::ContextMenuSourceType type) OVERRIDE;
  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE;
  virtual void Initialize(GtkWidget* expanded_container,
                          ui::FocusStoreGtk* focus_store) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual gboolean OnNativeViewFocusEvent(GtkWidget* widget,
                                          GtkDirectionType type,
                                          gboolean* return_value) OVERRIDE;

 private:
  // Sets the location of the web contents modal dialogs.
  CHROMEGTK_CALLBACK_1(ChromeWebContentsViewDelegateGtk, void,
                       OnSetFloatingPosition,
                       GtkAllocation*);

  // Contains |expanded_| as its GtkBin member.
  ui::ScopedGObject<GtkWidget>::Type floating_;

  // The UI for the web contents modal dialog currently displayed. This is owned
  // by WebContents, not the view.
  GtkWidget* web_contents_modal_dialog_;

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  scoped_ptr<RenderViewContextMenuGtk> context_menu_;

  // The chrome specific delegate that receives events from WebDragDestGtk.
  scoped_ptr<WebDragBookmarkHandlerGtk> bookmark_handler_gtk_;

  content::WebContents* web_contents_;

  GtkWidget* expanded_container_;
  ui::FocusStoreGtk* focus_store_;
};

#endif  // CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_GTK_H_
