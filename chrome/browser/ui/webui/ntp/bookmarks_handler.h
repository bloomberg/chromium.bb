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

  // Callback for various chrome.send() messages.
  void HandleGetBookmarksData(const base::ListValue* args);
  void HandleRemoveBookmark(const base::ListValue* args);

  // Register NTP preferences.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // A weak reference to the current bookmarks model.
  BookmarkModel* model_;  // weak

  // True if the DOM layer is ready.
  bool dom_ready_;

  // Used as a flag to ignore changes from the current page's actions.
  bool ignore_change_notifications_;

  DISALLOW_COPY_AND_ASSIGN(BookmarksHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_BOOKMARKS_HANDLER_H_
