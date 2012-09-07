// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_MAC_H_

#if defined(__OBJC__)

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"

class RenderViewContextMenuMac;
class WebDragBookmarkHandlerMac;

namespace content {
class WebContents;
}

// A chrome/ specific class that extends WebContentsViewMac with features that
// live in chrome/.
class ChromeWebContentsViewDelegateMac
    : public content::WebContentsViewDelegate {
 public:
  explicit ChromeWebContentsViewDelegateMac(content::WebContents* web_contents);
  virtual ~ChromeWebContentsViewDelegateMac();

  // Overridden from WebContentsViewDelegate:
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
      CreateRenderWidgetHostViewDelegate(
          content::RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params,
      const content::ContextMenuSourceType& type) OVERRIDE;

 private:
  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuMac> context_menu_;

  // The chrome specific delegate that receives events from WebDragDestMac.
  scoped_ptr<WebDragBookmarkHandlerMac> bookmark_handler_;

  // The WebContents that owns the view.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateMac);
};

#endif  // __OBJC__

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_MAC_H_
