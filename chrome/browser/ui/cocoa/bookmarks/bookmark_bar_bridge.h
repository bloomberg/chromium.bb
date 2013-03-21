// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C++ bridge class between Chromium and Cocoa to connect the
// Bookmarks (model) with the Bookmark Bar (view).
//
// There is exactly one BookmarkBarBridge per BookmarkBarController /
// BrowserWindowController / Browser.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_BRIDGE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"

class Profile;
@class BookmarkBarController;

class BookmarkBarBridge : public BookmarkModelObserver {
 public:
  BookmarkBarBridge(Profile* profile,
                    BookmarkBarController* controller,
                    BookmarkModel* model);
  virtual ~BookmarkBarBridge();

  // Overridden from BookmarkModelObserver
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
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE;
  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) OVERRIDE;

 private:
  BookmarkBarController* controller_;  // weak; owns me
  BookmarkModel* model_;  // weak; it is owned by a Profile.
  bool batch_mode_;

  // Needed to react to kShowAppsShortcutInBookmarkBar changes.
  PrefChangeRegistrar profile_pref_registrar_;

  // Updates the visibility of the apps shortcut based on the pref value.
  void OnAppsPageShortcutVisibilityChanged();

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_BRIDGE_H_
