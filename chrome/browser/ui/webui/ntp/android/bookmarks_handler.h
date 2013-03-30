// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_BOOKMARKS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_BOOKMARKS_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/ui/webui/ntp/android/partner_bookmarks_shim.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages related to the bookmarks.
//
// In Javascript if getBookmarks() is called without any parameter, the 'Other
// Bookmark' folder and bookmark bar's bookmarks and folders are returned.
// If getBookmarks() is called with a valid bookmark folder id, the given
// folder's bookmarks and sub folders of are returned.
//
// All bookmarks and subfolder is returned by bookmarks() javascript callback
// function.
// The returned field 'folder' indicates whether the data is a folder. The
// returned field 'root' indicates whether or not the bookmark list that was
// returned is the root list or not.  Besides these fields, a folder has id
// and title fields; A bookmark has url and title fields.
//
// A sample result looks like:
// {
//   title: 'Bookmark Bar',
//   id: '1',
//   root: true,
//   bookmarks: [
//     {
//       title: 'Cake',
//       url: 'http://www.google.com',
//       folder: false
//     },
//     {
//       title: 'Puppies',
//       folder: true,
//       id: '2'
//     }
//   ]
// }
class BookmarksHandler : public content::WebUIMessageHandler,
                         public BaseBookmarkModelObserver,
                         public PartnerBookmarksShim::Observer {
 public:
  BookmarksHandler();
  virtual ~BookmarksHandler();

  // WebUIMessageHandler override and implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "getBookmarks" message.
  void HandleGetBookmarks(const ListValue* args);
  // Callback for the "deleteBookmark" message.
  void HandleDeleteBookmark(const ListValue* args);
  // Callback for the "editBookmark" message.
  void HandleEditBookmark(const ListValue* args);
  // Callback for the "createHomeScreenBookmarkShortcut" message.  Used when
  // creating a shortcut on the home screen that should open the bookmark
  // specified in |args|.
  void HandleCreateHomeScreenBookmarkShortcut(const base::ListValue* args);

  // Notify the UI that a change occurred to the bookmark model.
  virtual void NotifyModelChanged(const DictionaryValue& status);

  // Override the methods of BookmarkModelObserver
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE;
  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkAllNodesRemoved(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeAdded(
      BookmarkModel* model, const BookmarkNode* parent, int index) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;

  // Override the methods of PartnerBookmarksShim::Observer
  virtual void PartnerShimLoaded(PartnerBookmarksShim* shim) OVERRIDE;
  virtual void ShimBeingDeleted(PartnerBookmarksShim* shim) OVERRIDE;

 private:
  // The bookmark model being observed (if it has been attached).
  BookmarkModel* bookmark_model_;

  // Information about the Partner bookmarks (must check for IsLoaded())
  PartnerBookmarksShim* partner_bookmarks_shim_;

  // Whether the bookmark data has been requested by the UI yet.
  bool bookmark_data_requested_;

  // Indicates that extensive changes to the BookmarkModel is on-going.
  bool extensive_changes_;

  // Used for loading bookmark node.
  CancelableTaskTracker cancelable_task_tracker_;

  // Generates the string encoded ID to be used by the NTP.
  std::string GetBookmarkIdForNtp(const BookmarkNode* node);

  // Sets the necessary parent information in the response object to be sent
  // to the UI renderer.
  void SetParentInBookmarksResult(const BookmarkNode* parent,
                                  DictionaryValue* result);

  // Convert the given bookmark |node| into a dictionary format to be returned
  // to JavaScript.
  void PopulateBookmark(const BookmarkNode* node, ListValue* result);

  // Given a bookmark folder node, |folder|, populate the |result| with the
  // structured JavaScript-formatted data regarding the folder.
  void PopulateBookmarksInFolder(const BookmarkNode* folder,
                                 DictionaryValue* result);

  // Sends all bookmarks and sub folders in the given folder back to the NTP.
  void QueryBookmarkFolder(const int64& id, bool is_partner_bookmark);

  // Sends bookmark bar's bookmarks and sub folders and other folders back to
  // NTP.
  void QueryInitialBookmarks();

  // Sends the result back to Javascript
  void SendResult(const DictionaryValue& result);

  // Called once the favicon is loaded during creation of the bookmark shortcuts
  // and is available for use.
  void OnShortcutFaviconDataAvailable(
      const BookmarkNode* node,
      const history::FaviconBitmapResult& bitmap_result);

  DISALLOW_COPY_AND_ASSIGN(BookmarksHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_BOOKMARKS_HANDLER_H_
