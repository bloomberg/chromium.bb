// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_SERIALIZED_NAVIGATION_ENTRY_H_
#define COMPONENTS_SESSIONS_SERIALIZED_NAVIGATION_ENTRY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "components/sessions/sessions_export.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"

class Pickle;
class PickleIterator;

namespace content {
class BrowserContext;
class NavigationEntry;
}

namespace sync_pb {
class TabNavigation;
}

namespace sessions {

class SerializedNavigationEntryTestHelper;

// The key used to store search terms data in the NavigationEntry.
SESSIONS_EXPORT extern const char kSearchTermsKey[];

// SerializedNavigationEntry is a "freeze-dried" version of NavigationEntry.  It
// contains the data needed to restore a NavigationEntry during session restore
// and tab restore, and it can also be pickled and unpickled.  It is also
// convertible to a sync protocol buffer for session syncing.
//
// Default copy constructor and assignment operator welcome.
class SESSIONS_EXPORT SerializedNavigationEntry {
 public:
  // Creates an invalid (index < 0) SerializedNavigationEntry.
  SerializedNavigationEntry();
  ~SerializedNavigationEntry();

  // Construct a SerializedNavigationEntry for a particular index from the given
  // NavigationEntry.
  static SerializedNavigationEntry FromNavigationEntry(
      int index,
      const content::NavigationEntry& entry);

  // Construct a SerializedNavigationEntry for a particular index from a sync
  // protocol buffer.  Note that the sync protocol buffer doesn't contain all
  // SerializedNavigationEntry fields.  Also, the timestamp of the returned
  // SerializedNavigationEntry is nulled out, as we assume that the protocol
  // buffer is from a foreign session.
  static SerializedNavigationEntry FromSyncData(
      int index,
      const sync_pb::TabNavigation& sync_data);

  // Note that not all SerializedNavigationEntry fields are preserved.
  // |max_size| is the max number of bytes to write.
  void WriteToPickle(int max_size, Pickle* pickle) const;
  bool ReadFromPickle(PickleIterator* iterator);

  // Convert this SerializedNavigationEntry into a NavigationEntry with the
  // given page ID and context.  The NavigationEntry will have a transition type
  // of PAGE_TRANSITION_RELOAD and a new unique ID.
  scoped_ptr<content::NavigationEntry> ToNavigationEntry(
      int page_id,
      content::BrowserContext* browser_context) const;

  // Convert this navigation into its sync protocol buffer equivalent.  Note
  // that the protocol buffer doesn't contain all SerializedNavigationEntry
  // fields.
  sync_pb::TabNavigation ToSyncData() const;

  // The index in the NavigationController. This SerializedNavigationEntry is
  // valid only when the index is non-negative.
  int index() const { return index_; }
  void set_index(int index) { index_ = index; }

  // Accessors for some fields taken from NavigationEntry.
  int unique_id() const { return unique_id_; }
  const GURL& virtual_url() const { return virtual_url_; }
  const string16& title() const { return title_; }
  const std::string& content_state() const { return content_state_; }
  const string16& search_terms() const { return search_terms_; }
  const GURL& favicon_url() const { return favicon_url_; }
  const content::Referrer& referrer() const { return referrer_; }
  content::PageTransition transition_type() const {
    return transition_type_;
  }
  bool has_post_data() const { return has_post_data_; }
  int64 post_id() const { return post_id_; }
  const GURL& original_request_url() const { return original_request_url_; }
  bool is_overriding_user_agent() const { return is_overriding_user_agent_; }
  base::Time timestamp() const { return timestamp_; }

  // Converts a set of SerializedNavigationEntrys into a list of
  // NavigationEntrys with sequential page IDs and the given context. The caller
  // owns the returned NavigationEntrys.
  static std::vector<content::NavigationEntry*> ToNavigationEntries(
      const std::vector<SerializedNavigationEntry>& navigations,
      content::BrowserContext* browser_context);

 private:
  friend class SerializedNavigationEntryTestHelper;

  // Index in the NavigationController.
  int index_;

  // Member variables corresponding to NavigationEntry fields.
  int unique_id_;
  content::Referrer referrer_;
  GURL virtual_url_;
  string16 title_;
  std::string content_state_;
  content::PageTransition transition_type_;
  bool has_post_data_;
  int64 post_id_;
  GURL original_request_url_;
  bool is_overriding_user_agent_;
  base::Time timestamp_;
  string16 search_terms_;
  GURL favicon_url_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_SERIALIZED_NAVIGATION_ENTRY_H_
