// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_AURA_H_
#define CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_AURA_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"

class WebDragBookmarkHandlerAura;

namespace content {
class WebContents;
}

// A chrome specific class that extends TabContentsViewViews with features like
// custom drag and drop for bookmarks.
class ChromeWebContentsViewDelegateAura
    : public content::WebContentsViewDelegate {
 public:
  explicit ChromeWebContentsViewDelegateAura(
    content::WebContents* web_contents);
  virtual ~ChromeWebContentsViewDelegateAura();

  // Overridden from WebContentsViewDelegate:
  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

 private:
  content::WebContents* web_contents_;

  // The chrome specific delegate for bookmark DnD.
  scoped_ptr<WebDragBookmarkHandlerAura> bookmark_handler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateAura);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_AURA_H_
