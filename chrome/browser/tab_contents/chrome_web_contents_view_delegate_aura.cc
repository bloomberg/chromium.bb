// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/chrome_web_contents_view_delegate_aura.h"

#include "chrome/browser/tab_contents/web_drag_bookmark_handler_aura.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

ChromeWebContentsViewDelegateAura::ChromeWebContentsViewDelegateAura(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      bookmark_handler_(new WebDragBookmarkHandlerAura()) {
}

ChromeWebContentsViewDelegateAura::~ChromeWebContentsViewDelegateAura() {
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateAura::GetDragDestDelegate() {
  // We install a chrome specific handler to intercept bookmark drags for the
  // bookmark manager/extension API.
  return bookmark_handler_.get();
}

void ChromeWebContentsViewDelegateAura::ShowContextMenu(
    const content::ContextMenuParams& params) {
}
