// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted_memory.h"
#include "base/stack_container.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/history/snippet.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/ref_counted_util.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"

namespace history {

// Forward declaration for friend statements.
class HistoryBackend;
class URLDatabase;

// Structure to hold redirect lists for URLs.  For a redirect chain
// A -> B -> C, and entry in the map would look like "A => {B -> C}".
typedef std::map<GURL, scoped_refptr<RefCountedVector<GURL> > > RedirectMap;

// Container for a list of URLs.
typedef std::vector<GURL> RedirectList;

typedef int64 StarID;  // Unique identifier for star entries.
typedef int64 UIStarID;  // Identifier for star entries that come from the UI.
typedef int64 DownloadID;   // Identifier for a download.
typedef int64 FaviconID;  // For favicons.
typedef int64 SegmentID;  // URL segments for the most visited view.
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

  // The row ID of this URL. Immutable except for the database which sets it
  // when it pulls them out.
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

// The enumeration of all possible sources of visits is listed below.
// The source will be propogated along with a URL or a visit item
// and eventually be stored in the history database,
// visit_source table specifically.
// Different from page transition types, they describe the origins of visits.
// (Warning): Please don't change any existing values while it is ok to add
// new values when needed.
typedef enum {
  SOURCE_SYNCED = 0,         // Synchronized from somewhere else.
  SOURCE_BROWSED = 1,        // User browsed.
  SOURCE_EXTENSION = 2,      // Added by an externsion.
  SOURCE_FIREFOX_IMPORTED = 3,
  SOURCE_IE_IMPORTED = 4,
  SOURCE_SAFARI_IMPORTED = 5,
} VisitSource;

typedef int64 VisitID;
// Structure to hold the mapping between each visit's id and its source.
typedef std::map<VisitID, VisitSource> VisitSourceMap;

// VisitRow -------------------------------------------------------------------

// Holds all information associated with a specific visit. A visit holds time
// and referrer information for one time a URL is visited.
class VisitRow {
 public:
  VisitRow();
  VisitRow(URLID arg_url_id,
           base::Time arg_visit_time,
           VisitID arg_referring_visit,
           PageTransition::Type arg_transition,
           SegmentID arg_segment_id);
  ~VisitRow();

  // ID of this row (visit ID, used a a referrer for other visits).
  VisitID visit_id;

  // Row ID into the URL table of the URL that this page is.
  URLID url_id;

  base::Time visit_time;

  // Indicates another visit that was the referring page for this one.
  // 0 indicates no referrer.
  VisitID referring_visit;

  // A combination of bits from PageTransition.
  PageTransition::Type transition;

  // The segment id (see visitsegment_database.*).
  // If 0, the segment id is null in the table.
  SegmentID segment_id;

  // True when this visit has indexed data for it. We try to keep this in sync
  // with the full text index: when we add or remove things from there, we will
  // update the visit table as well. However, that file could get deleted, or
  // out of sync in various ways, so this flag should be false when things
  // change.
  bool is_indexed;

  // Compares two visits based on dates, for sorting.
  bool operator<(const VisitRow& other) {
    return visit_time < other.visit_time;
  }

  // We allow the implicit copy constuctor and operator=.
};

// We pass around vectors of visits a lot
typedef std::vector<VisitRow> VisitVector;

// Favicons -------------------------------------------------------------------

// Used by the importer to set favicons for imported bookmarks.
struct ImportedFaviconUsage {
  ImportedFaviconUsage();
  ~ImportedFaviconUsage();

  // The URL of the favicon.
  GURL favicon_url;

  // The raw png-encoded data.
  std::vector<unsigned char> png_data;

  // The list of URLs using this favicon.
  std::set<GURL> urls;
};

// PageVisit ------------------------------------------------------------------

// Represents a simplified version of a visit for external users. Normally,
// views are only interested in the time, and not the other information
// associated with a VisitRow.
struct PageVisit {
  URLID page_id;
  base::Time visit_time;
};

// StarredEntry ---------------------------------------------------------------

// StarredEntry represents either a starred page, or a star grouping (where
// a star grouping consists of child starred entries). Use the type to
// determine the type of a particular entry.
//
// The database internally uses the id field to uniquely identify a starred
// entry. On the other hand, the UI, which is anything routed through
// HistoryService and HistoryBackend (including BookmarkBarView), uses the
// url field to uniquely identify starred entries of type URL and the group_id
// field to uniquely identify starred entries of type USER_GROUP. For example,
// HistoryService::UpdateStarredEntry identifies the entry by url (if the
// type is URL) or group_id (if the type is not URL).
struct StarredEntry {
  enum Type {
    // Type represents a starred URL (StarredEntry).
    URL,

    // The bookmark bar grouping.
    BOOKMARK_BAR,

    // User created group.
    USER_GROUP,

    // The "other bookmarks" folder that holds uncategorized bookmarks.
    OTHER
  };

  StarredEntry();
  ~StarredEntry();

  void Swap(StarredEntry* other);

  // Unique identifier of this entry.
  StarID id;

  // Title.
  string16 title;

  // When this was added.
  base::Time date_added;

  // Group ID of the star group this entry is in. If 0, this entry is not
  // in a star group.
  UIStarID parent_group_id;

  // Unique identifier for groups. This is assigned by the UI.
  //
  // WARNING: this is NOT the same as id, id is assigned by the database,
  // this is assigned by the UI. See note about StarredEntry for more info.
  UIStarID group_id;

  // Visual order within the parent. Only valid if group_id is not 0.
  int visual_order;

  // Type of this entry (see enum).
  Type type;

  // If type == URL, this is the URL of the page that was starred.
  GURL url;

  // If type == URL, this is the ID of the URL of the primary page that was
  // starred.
  URLID url_id;

  // Time the entry was last modified. This is only used for groups and
  // indicates the last time a URL was added as a child to the group.
  base::Time date_folder_modified;
};

// URLResult -------------------------------------------------------------------

class URLResult : public URLRow {
 public:
  URLResult();
  URLResult(const GURL& url, base::Time visit_time);
  // Constructor that create a URLResult from the specified URL and title match
  // positions from title_matches.
  URLResult(const GURL& url, const Snippet::MatchPositions& title_matches);
  ~URLResult();

  base::Time visit_time() const { return visit_time_; }
  void set_visit_time(base::Time visit_time) { visit_time_ = visit_time; }

  const Snippet& snippet() const { return snippet_; }

  // If this is a title match, title_match_positions contains an entry for
  // every word in the title that matched one of the query parameters. Each
  // entry contains the start and end of the match.
  const Snippet::MatchPositions& title_match_positions() const {
    return title_match_positions_;
  }

  void SwapResult(URLResult* other);

 private:
  friend class HistoryBackend;

  // The time that this result corresponds to.
  base::Time visit_time_;

  // These values are typically set by HistoryBackend.
  Snippet snippet_;
  Snippet::MatchPositions title_match_positions_;

  // We support the implicit copy constructor and operator=.
};

// QueryResults ----------------------------------------------------------------

// Encapsulates the results of a history query. It supports an ordered list of
// URLResult objects, plus an efficient way of looking up the index of each time
// a given URL appears in those results.
class QueryResults {
 public:
  typedef std::vector<URLResult*> URLResultVector;

  QueryResults();
  ~QueryResults();

  // Indicates the first time that the query includes results for (queries are
  // clipped at the beginning, so it will always include to the end of the time
  // queried).
  //
  // If the number of results was clipped as a result of the max count, this
  // will be the time of the first query returned. If there were fewer results
  // than we were allowed to return, this represents the first date considered
  // in the query (this will be before the first result if there was time
  // queried with no results).
  //
  // TODO(brettw): bug 1203054: This field is not currently set properly! Do
  // not use until the bug is fixed.
  base::Time first_time_searched() const { return first_time_searched_; }
  void set_first_time_searched(base::Time t) { first_time_searched_ = t; }
  // Note: If you need end_time_searched, it can be added.

  void set_reached_beginning(bool reached) { reached_beginning_ = reached; }
  bool reached_beginning() { return reached_beginning_; }

  size_t size() const { return results_.size(); }
  bool empty() const { return results_.empty(); }

  URLResult& operator[](size_t i) { return *results_[i]; }
  const URLResult& operator[](size_t i) const { return *results_[i]; }

  URLResultVector::const_iterator begin() const { return results_.begin(); }
  URLResultVector::const_iterator end() const { return results_.end(); }
  URLResultVector::const_reverse_iterator rbegin() const {
    return results_.rbegin();
  }
  URLResultVector::const_reverse_iterator rend() const {
    return results_.rend();
  }

  // Returns a pointer to the beginning of an array of all matching indices
  // for entries with the given URL. The array will be |*num_matches| long.
  // |num_matches| can be NULL if the caller is not interested in the number of
  // results (commonly it will only be interested in the first one and can test
  // the pointer for NULL).
  //
  // When there is no match, it will return NULL and |*num_matches| will be 0.
  const size_t* MatchesForURL(const GURL& url, size_t* num_matches) const;

  // Swaps the current result with another. This allows ownership to be
  // efficiently transferred without copying.
  void Swap(QueryResults* other);

  // Adds the given result to the map, using swap() on the members to avoid
  // copying (there are a lot of strings and vectors). This means the parameter
  // object will be cleared after this call.
  void AppendURLBySwapping(URLResult* result);

  // Appends a new result set to the other. The |other| results will be
  // destroyed because the pointer ownership will just be transferred. When
  // |remove_dupes| is set, each URL that appears in this array will be removed
  // from the |other| array before appending.
  void AppendResultsBySwapping(QueryResults* other, bool remove_dupes);

  // Removes all instances of the given URL from the result set.
  void DeleteURL(const GURL& url);

  // Deletes the given range of items in the result set.
  void DeleteRange(size_t begin, size_t end);

 private:
  // Maps the given URL to a list of indices into results_ which identify each
  // time an entry with that URL appears. Normally, each URL will have one or
  // very few indices after it, so we optimize this to use statically allocated
  // memory when possible.
  typedef std::map<GURL, StackVector<size_t, 4> > URLToResultIndices;

  // Inserts an entry into the |url_to_results_| map saying that the given URL
  // is at the given index in the results_.
  void AddURLUsageAtIndex(const GURL& url, size_t index);

  // Adds |delta| to each index in url_to_results_ in the range [begin,end]
  // (this is inclusive). This is used when inserting or deleting.
  void AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta);

  base::Time first_time_searched_;

  // Whether the query reaches the beginning of the database.
  bool reached_beginning_;

  // The ordered list of results. The pointers inside this are owned by this
  // QueryResults object.
  URLResultVector results_;

  // Maps URLs to entries in results_.
  URLToResultIndices url_to_results_;

  DISALLOW_COPY_AND_ASSIGN(QueryResults);
};

// QueryOptions ----------------------------------------------------------------

struct QueryOptions {
  QueryOptions();

  // The time range to search for matches in.
  //
  // This will match only the one recent visit of a URL.  For text search
  // queries, if the URL was visited in the given time period, but has also been
  // visited more recently than that, it will not be returned. When the text
  // query is empty, this will return the most recent visit within the time
  // range.
  //
  // As a special case, if both times are is_null(), then the entire database
  // will be searched. However, if you set one, you must set the other.
  //
  // The beginning is inclusive and the ending is exclusive.
  base::Time begin_time;
  base::Time end_time;

  // Sets the query time to the last |days_ago| days to the present time.
  void SetRecentDayRange(int days_ago);

  // The maximum number of results to return. The results will be sorted with
  // the most recent first, so older results may not be returned if there is not
  // enough room. When 0, this will return everything (the default).
  int max_count;
};

// KeywordSearchTermVisit -----------------------------------------------------

// KeywordSearchTermVisit is returned from GetMostRecentKeywordSearchTerms. It
// gives the time and search term of the keyword visit.
struct KeywordSearchTermVisit {
  KeywordSearchTermVisit();
  ~KeywordSearchTermVisit();

  // The time of the visit.
  base::Time time;

  // The search term that was used.
  string16 term;
};

// KeywordSearchTermRow --------------------------------------------------------

// Used for URLs that have a search term associated with them.
struct KeywordSearchTermRow {
  KeywordSearchTermRow();
  ~KeywordSearchTermRow();

  // ID of the keyword.
  TemplateURLID keyword_id;

  // ID of the url.
  URLID url_id;

  // The search term that was used.
  string16 term;
};

// MostVisitedURL --------------------------------------------------------------

// Holds the per-URL information of the most visited query.
struct MostVisitedURL {
  MostVisitedURL();
  MostVisitedURL(const GURL& in_url,
                 const GURL& in_favicon_url,
                 const string16& in_title);
  ~MostVisitedURL();

  GURL url;
  GURL favicon_url;
  string16 title;

  RedirectList redirects;

  bool operator==(const MostVisitedURL& other) {
    return url == other.url;
  }
};

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
class HistoryAddPageArgs
    : public base::RefCountedThreadSafe<HistoryAddPageArgs> {
 public:
  HistoryAddPageArgs(const GURL& arg_url,
                     base::Time arg_time,
                     const void* arg_id_scope,
                     int32 arg_page_id,
                     const GURL& arg_referrer,
                     const history::RedirectList& arg_redirects,
                     PageTransition::Type arg_transition,
                     VisitSource arg_source,
                     bool arg_did_replace_entry);

  // Returns a new HistoryAddPageArgs that is a copy of this (ref count is
  // of course reset). Ownership of returned object passes to caller.
  HistoryAddPageArgs* Clone() const;

  GURL url;
  base::Time time;

  const void* id_scope;
  int32 page_id;

  GURL referrer;
  history::RedirectList redirects;
  PageTransition::Type transition;
  VisitSource visit_source;
  bool did_replace_entry;

 private:
  friend class base::RefCountedThreadSafe<HistoryAddPageArgs>;

  ~HistoryAddPageArgs();

  DISALLOW_COPY_AND_ASSIGN(HistoryAddPageArgs);
};

// TopSites -------------------------------------------------------------------

typedef std::vector<MostVisitedURL> MostVisitedURLList;

// Used by TopSites to store the thumbnails.
struct Images {
  Images();
  ~Images();

  scoped_refptr<RefCountedBytes> thumbnail;
  ThumbnailScore thumbnail_score;

  // TODO(brettw): this will eventually store the favicon.
  // scoped_refptr<RefCountedBytes> favicon;
};

typedef std::vector<MostVisitedURL> MostVisitedURLList;

struct MostVisitedURLWithRank {
  MostVisitedURL url;
  int rank;
};

typedef std::vector<MostVisitedURLWithRank> MostVisitedURLWithRankList;

struct TopSitesDelta {
  TopSitesDelta();
  ~TopSitesDelta();

  MostVisitedURLList deleted;
  MostVisitedURLWithRankList added;
  MostVisitedURLWithRankList moved;
};

typedef std::map<GURL, scoped_refptr<RefCountedBytes> > URLToThumbnailMap;

// Used when migrating most visited thumbnails out of history and into topsites.
struct ThumbnailMigration {
  ThumbnailMigration();
  ~ThumbnailMigration();

  MostVisitedURLList most_visited;
  URLToThumbnailMap url_to_thumbnail_map;
};

typedef std::map<GURL, Images> URLToImagesMap;

class MostVisitedThumbnails
    : public base::RefCountedThreadSafe<MostVisitedThumbnails> {
 public:
  MostVisitedThumbnails();
  virtual ~MostVisitedThumbnails();

  MostVisitedURLList most_visited;
  URLToImagesMap url_to_images_map;

 private:
  friend class base::RefCountedThreadSafe<MostVisitedThumbnails>;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedThumbnails);
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

// Defines the icon types. They are also stored in icon_type field of favicons
// table.
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

  // The type of icon.
  IconType icon_type;
};

// Defines the favicon stored in history backend.
struct FaviconData {
  FaviconData();
  ~FaviconData();

  // Returns true if the icon is known and image has data.
  bool is_valid();

  // Indicates whether the icon is known by the history backend.
  bool known_icon;

  // The bits of image.
  scoped_refptr<RefCountedMemory> image_data;

  // Indicates whether image is expired.
  bool expired;

  // The icon's URL.
  GURL icon_url;

  // The type of favicon.
  history::IconType icon_type;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
