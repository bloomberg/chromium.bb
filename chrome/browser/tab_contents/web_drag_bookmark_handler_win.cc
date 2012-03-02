// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_bookmark_handler_win.h"

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"

using content::WebContents;

WebDragBookmarkHandlerWin::WebDragBookmarkHandlerWin()
    : tab_(NULL) {
}

WebDragBookmarkHandlerWin::~WebDragBookmarkHandlerWin() {
}

void WebDragBookmarkHandlerWin::DragInitialize(WebContents* contents) {
  // Ideally we would want to initialize the the TabContentsWrapper member in
  // the constructor. We cannot do that as the WebDragTargetWin object is
  // created during the construction of the TabContents object.  The
  // TabContentsWrapper is created much later.
  DCHECK(tab_ ? (tab_->web_contents() == contents) : true);
  if (!tab_)
    tab_ = TabContentsWrapper::GetCurrentWrapperForContents(contents);
}

void WebDragBookmarkHandlerWin::OnDragOver(IDataObject* data_object) {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragOver(
          bookmark_drag_data);
  }
}

void WebDragBookmarkHandlerWin::OnDragEnter(IDataObject* data_object) {
  // This is non-null if web_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragEnter(
          bookmark_drag_data);
  }
}

void WebDragBookmarkHandlerWin::OnDrop(IDataObject* data_object) {
  // This is non-null if tab_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (tab_) {
    if (tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
      ui::OSExchangeData os_exchange_data(
          new ui::OSExchangeDataProviderWin(data_object));
      BookmarkNodeData bookmark_drag_data;
      if (bookmark_drag_data.Read(os_exchange_data)) {
        tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDrop(
            bookmark_drag_data);
      }
    }

    // Focus the target browser.
    Browser* browser = Browser::GetBrowserForController(
        &tab_->web_contents()->GetController(), NULL);
    if (browser)
      browser->window()->Show();
  }
}

void WebDragBookmarkHandlerWin::OnDragLeave(IDataObject* data_object) {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragLeave(
          bookmark_drag_data);
  }
}
