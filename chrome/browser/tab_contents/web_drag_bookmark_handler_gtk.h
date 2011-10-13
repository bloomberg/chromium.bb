// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_GTK_H_
#pragma once

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/tab_contents/web_drag_dest_delegate_gtk.h"

class TabContentsWrapper;

class WebDragBookmarkHandlerGtk : public WebDragDestDelegateGtk {
 public:
  WebDragBookmarkHandlerGtk();
  virtual ~WebDragBookmarkHandlerGtk();

  // Overridden from WebDragBookmarkDelegate:
  virtual void DragInitialize(TabContents* contents);
  virtual GdkAtom GetBookmarkTargetAtom() const;
  virtual void OnReceiveDataFromGtk(GtkSelectionData* data);
  virtual void OnReceiveProcessedData(const GURL& url,
                                      const string16& title);
  virtual void OnDragOver();
  virtual void OnDragEnter();
  virtual void OnDrop();
  virtual void OnDragLeave();

 private:
  // The TabContentsWrapper for |tab_contents_|.
  // Weak reference; may be NULL if the contents aren't contained in a wrapper
  // (e.g. WebUI dialogs).
  TabContentsWrapper* tab_;

  // The bookmark data for the current tab. This will be empty if there is not
  // a native bookmark drag (or we haven't gotten the data from the source yet).
  BookmarkNodeData bookmark_drag_data_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_GTK_H_
