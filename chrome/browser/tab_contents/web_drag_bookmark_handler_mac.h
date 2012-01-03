// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_MAC_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_MAC_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "content/browser/tab_contents/web_drag_dest_delegate.h"

class TabContentsWrapper;

// Chrome needs to intercept content drag events so it can dispatch them to the
// bookmarks and extensions system.
class WebDragBookmarkHandlerMac : public content::WebDragDestDelegate {
 public:
  WebDragBookmarkHandlerMac();
  virtual ~WebDragBookmarkHandlerMac();

  // Overridden from content::WebDragDestDelegate:
  virtual void DragInitialize(content::WebContents* contents) OVERRIDE;
  virtual void OnDragOver() OVERRIDE;
  virtual void OnDragEnter() OVERRIDE;
  virtual void OnDrop() OVERRIDE;
  virtual void OnDragLeave() OVERRIDE;

 private:
  // The TabContentsWrapper for |tab_contents_|.
  // Weak reference; may be NULL if the contents aren't contained in a wrapper
  // (e.g. WebUI dialogs).
  TabContentsWrapper* tab_;

  // The bookmark data for the current tab. This will be empty if there is not
  // a native bookmark drag.
  BookmarkNodeData bookmark_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(WebDragBookmarkHandlerMac);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_MAC_H_
