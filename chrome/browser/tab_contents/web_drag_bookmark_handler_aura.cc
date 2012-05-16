// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_bookmark_handler_aura.h"

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "webkit/glue/webdropdata.h"

using content::WebContents;

WebDragBookmarkHandlerAura::WebDragBookmarkHandlerAura()
    : tab_(NULL) {
}

WebDragBookmarkHandlerAura::~WebDragBookmarkHandlerAura() {
}

void WebDragBookmarkHandlerAura::DragInitialize(WebContents* contents) {
  // Ideally we would want to initialize the the TabContentsWrapper member in
  // the constructor. We cannot do that as the WebDragDest object is
  // created during the construction of the WebContents object.  The
  // TabContentsWrapper is created much later.
  DCHECK(tab_ ? (tab_->web_contents() == contents) : true);
  if (!tab_)
    tab_ = TabContentsWrapper::GetCurrentWrapperForContents(contents);
}

void WebDragBookmarkHandlerAura::OnDragOver() {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    if (bookmark_drag_data_.is_valid())
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragOver(
          bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerAura::OnReceiveDragData(
    const ui::OSExchangeData& data) {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    // Read the bookmark drag data and save it for use in later events in this
    // drag.
    bookmark_drag_data_.Read(data);
  }
}

void WebDragBookmarkHandlerAura::OnDragEnter() {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    if (bookmark_drag_data_.is_valid())
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragEnter(
          bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerAura::OnDrop() {
  if (tab_) {
    if (tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
      if (bookmark_drag_data_.is_valid()) {
        tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDrop(
            bookmark_drag_data_);
      }
    }

    // Focus the target browser.
    Browser* browser = browser::FindBrowserForController(
        &tab_->web_contents()->GetController(), NULL);
    if (browser)
      browser->window()->Show();
  }

  bookmark_drag_data_.Clear();
}

void WebDragBookmarkHandlerAura::OnDragLeave() {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    if (bookmark_drag_data_.is_valid())
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragLeave(
          bookmark_drag_data_);
  }

  bookmark_drag_data_.Clear();
}
