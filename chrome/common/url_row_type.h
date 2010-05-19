// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_URL_ROW_TYPE_H_
#define CHROME_COMMON_URL_ROW_TYPE_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

// This file includes the URLRow class and ImportedFavIconUsage struct, which
// need to be in the common directory because they're accessed by the importer
// process as well as the browser process.

namespace history {

// URLRow ---------------------------------------------------------------------

typedef int64 URLID;
typedef int64 FavIconID;  // For FavIcons.

// Holds all information globally associated with one URL (one row in the
// URL table).
//
// This keeps track of dirty bits, which are currently unused:
//
// TODO(brettw) the dirty bits are broken in a number of respects. First, the
// database will want to update them on a const object, so they need to be
// mutable.
//
// Second, there is a problem copying. If you make a copy of this structure
// (as we allow since we put this into vectors in various places) then the
// dirty bits will not be in sync for these copies.
class URLRow {
 public:
  URLRow() {
    Initialize();
  }

  explicit URLRow(const GURL& url) : url_(url) {
    // Initialize will not set the URL, so our initialization above will stay.
    Initialize();
  }

  // We need to be able to set the id of a URLRow that's being passed through
  // an IPC message.  This constructor should probably not be used otherwise.
  URLRow(const GURL& url, URLID id) : url_(url) {
    // Initialize will not set the URL, so our initialization above will stay.
    Initialize();
    // Initialize will zero the id_, so set it here.
    id_ = id;
  }

  virtual ~URLRow() {}

  URLID id() const { return id_; }
  const GURL& url() const { return url_; }

  const std::wstring& title() const {
    return title_;
  }
  void set_title(const std::wstring& title) {
    // The title is frequently set to the same thing, so we don't bother
    // updating unless the string has changed.
    if (title != title_) {
      title_ = title;
    }
  }

  int visit_count() const {
    return visit_count_;
  }
  void set_visit_count(int visit_count) {
    visit_count_ = visit_count;
  }

  // Number of times the URL was typed in the Omnibox.
  int typed_count() const {
    return typed_count_;
  }
  void set_typed_count(int typed_count) {
    typed_count_ = typed_count;
  }

  base::Time last_visit() const {
    return last_visit_;
  }
  void set_last_visit(base::Time last_visit) {
    last_visit_ = last_visit;
  }

  // If this is set, we won't autocomplete this URL.
  bool hidden() const {
    return hidden_;
  }
  void set_hidden(bool hidden) {
    hidden_ = hidden;
  }

  // ID of the favicon. A value of 0 means the favicon isn't known yet.
  FavIconID favicon_id() const { return favicon_id_; }
  void set_favicon_id(FavIconID favicon_id) {
    favicon_id_ = favicon_id;
  }

  // Swaps the contents of this URLRow with another, which allows it to be
  // destructively copied without memory allocations.
  // (Virtual because it's overridden by URLResult.)
  virtual void Swap(URLRow* other);

 private:
  // This class writes directly into this structure and clears our dirty bits
  // when reading out of the DB.
  friend class URLDatabase;
  friend class HistoryBackend;

  // Initializes all values that need initialization to their defaults.
  // This excludes objects which autoinitialize such as strings.
  void Initialize();

  // The row ID of this URL. Immutable except for the database which sets it
  // when it pulls them out.
  URLID id_;

  // The URL of this row. Immutable except for the database which sets it
  // when it pulls them out. If clients want to change it, they must use
  // the constructor to make a new one.
  GURL url_;

  std::wstring title_;

  // Total number of times this URL has been visited.
  int visit_count_;

  // Number of times this URL has been manually entered in the URL bar.
  int typed_count_;

  // The date of the last visit of this URL, which saves us from having to
  // loop up in the visit table for things like autocomplete and expiration.
  base::Time last_visit_;

  // Indicates this entry should now be shown in typical UI or queries, this
  // is usually for subframes.
  bool hidden_;

  // The ID of the favicon for this url.
  FavIconID favicon_id_;

  // We support the implicit copy constuctor and operator=.
};

// Favicons -------------------------------------------------------------------

// Used by the importer to set favicons for imported bookmarks.
struct ImportedFavIconUsage {
  // The URL of the favicon.
  GURL favicon_url;

  // The raw png-encoded data.
  std::vector<unsigned char> png_data;

  // The list of URLs using this favicon.
  std::set<GURL> urls;
};

}  // namespace history

#endif  // CHROME_COMMON_URL_ROW_TYPE_H_


