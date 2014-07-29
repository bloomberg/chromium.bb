// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"

class RenderViewContextMenu;
class RenderViewContextMenuViews;

namespace aura {
class Window;
}

namespace content {
class WebContents;
class WebDragDestDelegate;
class RenderFrameHost;
}

namespace views {
class FocusManager;
class Widget;
}

// A chrome specific class that extends WebContentsViewWin with features like
// focus management, which live in chrome.
class ChromeWebContentsViewDelegateViews
    : public content::WebContentsViewDelegate,
      public ContextMenuDelegate {
 public:
  explicit ChromeWebContentsViewDelegateViews(
      content::WebContents* web_contents);
  virtual ~ChromeWebContentsViewDelegateViews();

  // Overridden from WebContentsViewDelegate:
  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool Focus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;
  virtual void ShowContextMenu(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void SizeChanged(const gfx::Size& size) OVERRIDE;

  // Overridden from ContextMenuDelegate.
  virtual scoped_ptr<RenderViewContextMenu> BuildMenu(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void ShowMenu(scoped_ptr<RenderViewContextMenu> menu) OVERRIDE;

 private:
  aura::Window* GetActiveNativeView();
  views::Widget* GetTopLevelWidget();
  views::FocusManager* GetFocusManager();
  void SetInitialFocus();

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  scoped_ptr<RenderViewContextMenuViews> context_menu_;

  // The chrome specific delegate that receives events from WebDragDest.
  scoped_ptr<content::WebDragDestDelegate> bookmark_handler_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_
