// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_PARTNER_BOOKMARKS_SHIM_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_PARTNER_BOOKMARKS_SHIM_H_

#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// A shim that lives on top of a BookmarkModel that allows the injection of
// Partner bookmarks without submitting changes to the user configured bookmark
// model.
// Partner bookmarks folder is pseudo-injected as a subfolder to "attach node".
// Because we cannot modify the BookmarkModel, the following needs to
// be done on a client side:
// 1. bookmark_node->is_root() -> shim->IsRootNode(bookmark_node)
// 2. bookmark_node->parent() -> shim->GetParentOf(bookmark_node)
// 3. bookmark_model->GetNodeByID(id) -> shim->GetNodeByID(id)
class PartnerBookmarksShim {
 public:
  // Constructor is public for LazyInstance; DON'T CALL; use ::GetInstance().
  PartnerBookmarksShim();
  // Destructor is public for LazyInstance;
  ~PartnerBookmarksShim();

  // Returns the active instance of the shim.
  static PartnerBookmarksShim* GetInstance();

  // Resets back to the initial state.  Clears any attached observers, deletes
  // the partner bookmarks if any, and returns back to the unloaded state.
  void Reset();

  // Pseudo-injects partner bookmarks (if any) under the "attach_node".
  void AttachTo(BookmarkModel* bookmark_model,
                const BookmarkNode* attach_node);
  // Returns true if everything got loaded and attached
  bool IsLoaded() const;
  // Returns true if there are partner bookmarks
  bool HasPartnerBookmarks() const;

  // For "Loaded" and "ShimBeingDeleted" notifications
  class Observer {
   public:
    // Called when everything is loaded and attached
    virtual void PartnerShimLoaded(PartnerBookmarksShim*) {}
    // Called just before everything got destroyed
    virtual void ShimBeingDeleted(PartnerBookmarksShim*) {}
   protected:
    virtual ~Observer() {}
  };
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Replacements for BookmarkModel/BookmarkNode methods
  bool IsBookmarkEditable(const BookmarkNode* node);
  bool IsRootNode(const BookmarkNode* node) const;
  const BookmarkNode* GetNodeByID(int64 id, bool is_partner) const;
  const BookmarkNode* GetParentOf(const BookmarkNode* node) const;
  const BookmarkNode* get_attach_point() const { return attach_point_; }
  bool IsPartnerBookmark(const BookmarkNode* node);
  const BookmarkNode* GetPartnerBookmarksRoot() const;
  // Sets the root node of the partner bookmarks and notifies any observers that
  // the shim has now been loaded.  Takes ownership of |root_node|.
  void SetPartnerBookmarksRoot(BookmarkNode* root_node);

 private:
  const BookmarkNode* GetNodeByID(const BookmarkNode* parent, int64 id) const;

  scoped_ptr<BookmarkNode> partner_bookmarks_root_;
  BookmarkModel* bookmark_model_;
  const BookmarkNode* attach_point_;
  bool loaded_;  // Set only on UI thread

  // The observers.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PartnerBookmarksShim);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_PARTNER_BOOKMARKS_SHIM_H_
