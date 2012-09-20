// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMON_URL_DATABASE_URL_DATABASE_TYPES_H_
#define CHROME_BROWSER_COMMON_URL_DATABASE_URL_DATABASE_TYPES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/common/url_database/template_url_id.h"
#include "googleurl/src/gurl.h"

namespace history {

typedef int64 FaviconID;  // For favicons.
typedef int64 IconMappingID; // For page url and icon mapping.

// URLRow ---------------------------------------------------------------------

typedef int64 URLID;

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
  URLRow();

  explicit URLRow(const GURL& url);

  // We need to be able to set the id of a URLRow that's being passed through
  // an IPC message.  This constructor should probably not be used otherwise.
  URLRow(const GURL& url, URLID id);

  virtual ~URLRow();
  URLRow& operator=(const URLRow& other);

  URLID id() const { return id_; }

  // Sets the id of the row. The id should only be manually set when a row has
  // been retrieved from the history database or other dataset based on criteria
  // other than its id (i.e. by URL) and when the id has not yet been set in the
  // row.
  void set_id(URLID id) { id_ = id; }

  const GURL& url() const { return url_; }

  const string16& title() const {
    return title_;
  }
  void set_title(const string16& title) {
    // The title is frequently set to the same thing, so we don't bother
    // updating unless the string has changed.
    if (title != title_) {
      title_ = title;
    }
  }

  // The number of times this URL has been visited. This will often match the
  // number of entries in the visit table for this URL, but won't always. It's
  // really designed for autocomplete ranking, so some "useless" transitions
  // from the visit table aren't counted in this tally.
  int visit_count() const {
    return visit_count_;
  }
  void set_visit_count(int visit_count) {
    visit_count_ = visit_count;
  }

  // Number of times the URL was typed in the Omnibox. This "should" match
  // the number of TYPED transitions in the visit table. It's used primarily
  // for faster autocomplete ranking. If you need to know the actual number of
  // TYPED transitions, you should query the visit table since there could be
  // something out of sync.
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

  // Helper functor that determines if an URLRow refers to a given URL.
  class URLRowHasURL {
   public:
    explicit URLRowHasURL(const GURL& url) : url_(url) {}

    bool operator()(const URLRow& row) {
      return row.url() == url_;
    }

   private:
    const GURL& url_;
  };

 protected:
  // Swaps the contents of this URLRow with another, which allows it to be
  // destructively copied without memory allocations.
  void Swap(URLRow* other);

 private:
  // This class writes directly into this structure and clears our dirty bits
  // when reading out of the DB.
  friend class URLDatabase;
  friend class HistoryBackend;

  // Initializes all values that need initialization to their defaults.
  // This excludes objects which autoinitialize such as strings.
  void Initialize();

  // The row ID of this URL from the history database. This is immutable except
  // when retrieving the row from the database or when determining if the URL
  // referenced by the URLRow already exists in the database.
  URLID id_;

  // The URL of this row. Immutable except for the database which sets it
  // when it pulls them out. If clients want to change it, they must use
  // the constructor to make a new one.
  GURL url_;

  string16 title_;

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

  // We support the implicit copy constuctor and operator=.
};

typedef std::vector<URLRow> URLRows;

// KeywordSearchTermVisit -----------------------------------------------------

// KeywordSearchTermVisit is returned from GetMostRecentKeywordSearchTerms. It
// gives the time and search term of the keyword visit.
struct KeywordSearchTermVisit {
  KeywordSearchTermVisit();
  ~KeywordSearchTermVisit();

  string16 term;    // The search term that was used.
  int visits;       // The visit count.
  base::Time time;  // The time of the most recent visit.
};

// KeywordSearchTermRow --------------------------------------------------------

// Used for URLs that have a search term associated with them.
struct KeywordSearchTermRow {
  KeywordSearchTermRow();
  ~KeywordSearchTermRow();

  TemplateURLID keyword_id;  // ID of the keyword.
  URLID url_id;              // ID of the url.
  string16 term;             // The search term that was used.
};

// Autocomplete thresholds -----------------------------------------------------

// Constants which specify, when considered altogether, 'significant'
// history items. These are used to filter out insignificant items
// for consideration as autocomplete candidates.
extern const int kLowQualityMatchTypedLimit;
extern const int kLowQualityMatchVisitLimit;
extern const int kLowQualityMatchAgeLimitInDays;

// Returns the date threshold for considering an history item as significant.
base::Time AutocompleteAgeThreshold();

// Return true if |row| qualifies as an autocomplete candidate. If |time_cache|
// is_null() then this function determines a new time threshold each time it is
// called. Since getting system time can be costly (such as for cases where
// this function will be called in a loop over many history items), you can
// provide a non-null |time_cache| by simply initializing |time_cache| with
// AutocompleteAgeThreshold() (or any other desired time in the past).
bool RowQualifiesAsSignificant(const URLRow& row, const base::Time& threshold);

// Favicons -------------------------------------------------------------------

// Defines the icon types. They are also stored in icon_type field of favicons
// table.
// The values of the IconTypes are used to select the priority in which favicon
// data is returned in HistoryBackend and ThumbnailDatabase. Data for the
// largest IconType takes priority if data for multiple IconTypes is available.
enum IconType {
  INVALID_ICON = 0x0,
  FAVICON = 1 << 0,
  TOUCH_ICON = 1 << 1,
  TOUCH_PRECOMPOSED_ICON = 1 << 2
};

// Used for the mapping between the page and icon.
struct IconMapping {
  IconMapping();
  ~IconMapping();

  // The unique id of the mapping.
  IconMappingID mapping_id;

  // The url of a web page.
  GURL page_url;

  // The unique id of the icon.
  FaviconID icon_id;

  // The url of the icon.
  GURL icon_url;

  // The type of icon.
  IconType icon_type;
};

}  // namespace history

#endif  // CHROME_BROWSER_COMMON_URL_DATABASE_URL_DATABASE_TYPES_H_
