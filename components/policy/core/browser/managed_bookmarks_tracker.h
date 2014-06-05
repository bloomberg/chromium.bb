// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_MANAGED_BOOKMARKS_TRACKER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_MANAGED_BOOKMARKS_TRACKER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "components/policy/policy_export.h"

class BookmarkModel;
class BookmarkNode;
class BookmarkPermanentNode;
class GURL;
class PrefService;

namespace base {
class ListValue;
}

namespace policy {

// Tracks the Managed Bookmarks policy value and makes the managed_node() in
// the BookmarkModel follow the policy-defined bookmark tree.
class POLICY_EXPORT ManagedBookmarksTracker {
 public:
  typedef base::Callback<std::string()> GetManagementDomainCallback;

  // Shared constants used in the policy configuration.
  static const char kName[];
  static const char kUrl[];
  static const char kChildren[];

  ManagedBookmarksTracker(BookmarkModel* model,
                          PrefService* prefs,
                          const GetManagementDomainCallback& callback);
  ~ManagedBookmarksTracker();

  // Returns the initial list of managed bookmarks, which can be passed to
  // LoadInitial() to do the initial load.
  scoped_ptr<base::ListValue> GetInitialManagedBookmarks();

  // Loads the initial managed bookmarks in |list| into |folder|. New nodes
  // will be assigned IDs starting at |next_node_id|.
  // Returns the next node ID to use.
  static int64 LoadInitial(BookmarkNode* folder,
                           const base::ListValue* list,
                           int64 next_node_id);

  // Starts tracking the policy for updates to the managed bookmarks. Should
  // be called after loading the initial bookmarks.
  void Init(BookmarkPermanentNode* managed_node);

 private:
  void ReloadManagedBookmarks();
  void UpdateBookmarks(const BookmarkNode* folder, const base::ListValue* list);
  static bool LoadBookmark(const base::ListValue* list,
                           size_t index,
                           base::string16* title,
                           GURL* url,
                           const base::ListValue** children);

  BookmarkModel* model_;
  BookmarkPermanentNode* managed_node_;
  PrefService* prefs_;
  PrefChangeRegistrar registrar_;
  GetManagementDomainCallback get_management_domain_callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarksTracker);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_MANAGED_BOOKMARKS_TRACKER_H_
