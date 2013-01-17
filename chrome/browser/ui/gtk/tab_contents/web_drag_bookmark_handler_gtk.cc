// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tab_contents/web_drag_bookmark_handler_gtk.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"

using content::WebContents;

WebDragBookmarkHandlerGtk::WebDragBookmarkHandlerGtk()
    : bookmark_tab_helper_(NULL),
      web_contents_(NULL) {
}

WebDragBookmarkHandlerGtk::~WebDragBookmarkHandlerGtk() {}

void WebDragBookmarkHandlerGtk::DragInitialize(WebContents* contents) {
  bookmark_drag_data_.Clear();

  // Ideally we would want to initialize the the BookmarkTabHelper member in
  // the constructor. We cannot do that as the WebDragDestGtk object is
  // created during the construction of the WebContents object.  The
  // BookmarkTabHelper is created much later.
  web_contents_ = contents;
  if (!bookmark_tab_helper_)
    bookmark_tab_helper_ = BookmarkTabHelper::FromWebContents(contents);
}

GdkAtom WebDragBookmarkHandlerGtk::GetBookmarkTargetAtom() const {
  // For GTK, bookmark drag data is encoded as pickle and associated with
  // ui::CHROME_BOOKMARK_ITEM. See bookmark_utils::WriteBookmarksToSelection()
  // for details.
  return ui::GetAtomForTarget(ui::CHROME_BOOKMARK_ITEM);
}

void WebDragBookmarkHandlerGtk::OnReceiveDataFromGtk(GtkSelectionData* data) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  bookmark_drag_data_.ReadFromVector(
      bookmark_utils::GetNodesFromSelection(
          NULL, data,
          ui::CHROME_BOOKMARK_ITEM,
          profile, NULL, NULL));
  bookmark_drag_data_.SetOriginatingProfile(profile);
}

void WebDragBookmarkHandlerGtk::OnReceiveProcessedData(const GURL& url,
                                                       const string16& title) {
  bookmark_drag_data_.ReadFromTuple(url, title);
}

void WebDragBookmarkHandlerGtk::OnDragOver() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    bookmark_tab_helper_->bookmark_drag_delegate()->OnDragOver(
        bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerGtk::OnDragEnter() {
  // This is non-null if the web_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    bookmark_tab_helper_->bookmark_drag_delegate()->OnDragEnter(
        bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerGtk::OnDrop() {
  // This is non-null if web_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (bookmark_tab_helper_) {
    if (bookmark_tab_helper_->bookmark_drag_delegate()) {
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDrop(
          bookmark_drag_data_);
    }

    // Focus the target browser.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    if (browser)
      browser->window()->Show();
  }
}

void WebDragBookmarkHandlerGtk::OnDragLeave() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    bookmark_tab_helper_->bookmark_drag_delegate()->OnDragLeave(
        bookmark_drag_data_);
  }
}
