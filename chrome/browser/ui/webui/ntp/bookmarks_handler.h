// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_BOOKMARKS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_BOOKMARKS_HANDLER_H_
#pragma once

#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "content/browser/webui/web_ui.h"

class PrefService;
class Profile;

// The handler for Javascript messages related to the "bookmarks" view.
class BookmarksHandler : public WebUIMessageHandler,
                         public BookmarkModelObserver {
 public:
  explicit BookmarksHandler();
  virtual ~BookmarksHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // BookmarkModelObserver implementation.
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkImportBeginning(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkImportEnding(BookmarkModel* model) OVERRIDE;

  // Callback for the "createBookmark" message. This handler could be sent as
  // many as four arguments. The first argument is a stringified int64 with the
  // parent ID of the node we're going to create the bookmark in. The additional
  // 3 optional parameters start with an integer index denoting where the
  // bookmark is relative to its siblings, a string for the title (which is
  // otherwise blank), and a string of the url (without this we interpret as a
  // folder).
  void HandleCreateBookmark(const base::ListValue* args);

  // Callback for the "getBoomarksData" message.
  void HandleGetBookmarksData(const base::ListValue* args);

  // Callback for the "moveBookmark" message. The arguments passed to this
  // handler are the bookmark ID (as a string to be converted to int64), parent
  // ID (as a string to be converted to int64), and index to be inserted into
  // that level.
  void HandleMoveBookmark(const base::ListValue* args);

  // Callback for the "removeBookmark" message. The argument passed to this
  // handler is a stringified int64 bookmark ID of the node we want to delete
  // (can be either a bookmark or a folder).
  void HandleRemoveBookmark(const base::ListValue* args);

  // Register NTP preferences.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // A weak reference to the current bookmarks model.
  BookmarkModel* model_;  // weak

  // True if the DOM layer is ready.
  bool dom_ready_;

  // True when we are handling a chrome.send call.
  bool from_current_page_;

  DISALLOW_COPY_AND_ASSIGN(BookmarksHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_BOOKMARKS_HANDLER_H_
