// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_MANAGED_BOOKMARKS_SHIM_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_MANAGED_BOOKMARKS_SHIM_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"

class BookmarkNode;
class PrefService;

// A shim that lives on top of a BookmarkModel that allows the injection of
// policy managed bookmarks without submitting changes to the user configured
// bookmark model.
// Policy managed bookmarks live on a subfolder of the Mobile bookmarks called
// "Managed bookmarks" that isn't editable by the user.
class ManagedBookmarksShim {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnManagedBookmarksChanged() = 0;
  };

  // Will read managed bookmarks from the given PrefService. The preference
  // that contains the bookmarks is managed by policy, and is updated when the
  // policy changes.
  explicit ManagedBookmarksShim(PrefService* prefs);
  ~ManagedBookmarksShim();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if there exists at least one managed bookmark.
  bool HasManagedBookmarks() const;
  bool IsManagedBookmark(const BookmarkNode* node) const;
  const BookmarkNode* GetManagedBookmarksRoot() const;
  const BookmarkNode* GetNodeByID(int64 id) const;
  const BookmarkNode* GetParentOf(const BookmarkNode* node) const;

 private:
  void Reload();

  PrefService* prefs_;
  PrefChangeRegistrar registrar_;
  scoped_ptr<BookmarkNode> root_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarksShim);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_MANAGED_BOOKMARKS_SHIM_H_
