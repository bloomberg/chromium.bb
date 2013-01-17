// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/web_drag_bookmark_handler_win.h"

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "webkit/glue/webdropdata.h"

using content::WebContents;

WebDragBookmarkHandlerWin::WebDragBookmarkHandlerWin()
    : bookmark_tab_helper_(NULL),
      web_contents_(NULL) {
}

WebDragBookmarkHandlerWin::~WebDragBookmarkHandlerWin() {
}

void WebDragBookmarkHandlerWin::DragInitialize(WebContents* contents) {
  // Ideally we would want to initialize the the BookmarkTabHelper member in
  // the constructor. We cannot do that as the WebDragTargetWin object is
  // created during the construction of the WebContents object.  The
  // BookmarkTabHelper is created much later.
  web_contents_ = contents;
  if (!bookmark_tab_helper_)
    bookmark_tab_helper_ = BookmarkTabHelper::FromWebContents(contents);
}

void WebDragBookmarkHandlerWin::OnDragOver(IDataObject* data_object) {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDragOver(
          bookmark_drag_data);
  }
}

void WebDragBookmarkHandlerWin::OnDragEnter(IDataObject* data_object) {
  // This is non-null if the web_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDragEnter(
          bookmark_drag_data);
  }
}

void WebDragBookmarkHandlerWin::OnDrop(IDataObject* data_object) {
  // This is non-null if the web_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (bookmark_tab_helper_) {
    if (bookmark_tab_helper_->bookmark_drag_delegate()) {
      ui::OSExchangeData os_exchange_data(
          new ui::OSExchangeDataProviderWin(data_object));
      BookmarkNodeData bookmark_drag_data;
      if (bookmark_drag_data.Read(os_exchange_data)) {
        bookmark_tab_helper_->bookmark_drag_delegate()->OnDrop(
            bookmark_drag_data);
      }
    }

    // Focus the target browser.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    if (browser)
      browser->window()->Show();
  }
}

void WebDragBookmarkHandlerWin::OnDragLeave(IDataObject* data_object) {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDragLeave(
          bookmark_drag_data);
  }
}

bool WebDragBookmarkHandlerWin::AddDragData(const WebDropData& drop_data,
                                            ui::OSExchangeData* data) {
  if (!drop_data.url.SchemeIs(chrome::kJavaScriptScheme))
    return false;

  // We don't want to allow javascript URLs to be dragged to the desktop,
  // but we do want to allow them to be added to the bookmarks bar
  // (bookmarklets). So we create a fake bookmark entry (BookmarkNodeData
  // object) which explorer.exe cannot handle, and write the entry to data.
  BookmarkNodeData::Element bm_elt;
  bm_elt.is_url = true;
  bm_elt.url = drop_data.url;
  bm_elt.title = drop_data.url_title;

  BookmarkNodeData bm_drag_data;
  bm_drag_data.elements.push_back(bm_elt);

  // Pass in NULL as the profile so that the bookmark always adds the url
  // rather than trying to move an existing url.
  bm_drag_data.Write(NULL, data);

  return true;
}
