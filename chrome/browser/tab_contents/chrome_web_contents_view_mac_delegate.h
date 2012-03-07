// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_MAC_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_MAC_DELEGATE_H_
#pragma once

#if defined(__OBJC__)

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_view_mac_delegate.h"

class RenderViewContextMenuMac;
class WebDragBookmarkHandlerMac;

namespace content {
class WebContents;
}

// A chrome/ specific class that extends WebContentsViewMac with features that
// live in chrome/.
class ChromeWebContentsViewMacDelegate
    : public content::WebContentsViewMacDelegate {
 public:
  explicit ChromeWebContentsViewMacDelegate(content::WebContents* web_contents);
  virtual ~ChromeWebContentsViewMacDelegate();

  // Overridden from WebContentsViewMacDelegate:
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
      CreateRenderWidgetHostViewDelegate(
          content::RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual content::WebDragDestDelegate* DragDelegate() OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void NativeViewCreated(NSView* view) OVERRIDE;
  virtual void NativeViewDestroyed(NSView* view) OVERRIDE;

 private:
  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuMac> context_menu_;

  // The chrome specific delegate that receives events from WebDragDestMac.
  scoped_ptr<WebDragBookmarkHandlerMac> bookmark_handler_;

  // The WebContents that owns the view.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewMacDelegate);
};

#endif  // __OBJC__

namespace content {
class WebContents;
class WebContentsViewMacDelegate;
}

namespace chrome_web_contents_view_mac_delegate {
// Creates a ChromeWebContentsViewMacDelegate.
content::WebContentsViewMacDelegate* CreateWebContentsViewMacDelegate(
    content::WebContents* web_contents);
}

#endif  // CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_MAC_DELEGATE_H_
