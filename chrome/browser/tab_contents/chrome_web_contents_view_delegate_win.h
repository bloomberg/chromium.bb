// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"

class ConstrainedWindowViews;
class RenderViewContextMenuViews;
class WebDragBookmarkHandlerWin;

namespace content {
class WebContents;
}

namespace views {
class FocusManager;
class Widget;
}

// A chrome specific class that extends TabContentsViewWin with features like
// constrained windows, which live in chrome.
class ChromeWebContentsViewDelegateWin
    : public content::WebContentsViewDelegate {
 public:
  explicit ChromeWebContentsViewDelegateWin(content::WebContents* web_contents);
  virtual ~ChromeWebContentsViewDelegateWin();

  // Overridden from WebContentsViewDelegate:
  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool Focus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void SizeChanged(const gfx::Size& size) OVERRIDE;

 private:
  views::Widget* GetTopLevelWidget();
  views::FocusManager* GetFocusManager();
  void SetInitialFocus();

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  scoped_ptr<RenderViewContextMenuViews> context_menu_;

  // The chrome specific delegate that receives events from WebDragDest.
  scoped_ptr<WebDragBookmarkHandlerWin> bookmark_handler_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateWin);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_WIN_H_
