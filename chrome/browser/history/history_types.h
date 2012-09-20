// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stack_container.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/common/url_database/template_url_id.h"
#include "chrome/browser/common/url_database/url_database_types.h"
#include "chrome/browser/history/snippet.h"
#include "chrome/common/ref_counted_util.h"
#include "chrome/common/thumbnail_score.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"

class PageUsageData;

namespace history {

// Forward declaration for friend statements.
class HistoryBackend;
class URLDatabase;

// Structure to hold redirect lists for URLs.  For a redirect chain
// A -> B -> C, and entry in the map would look like "A => {B -> C}".
typedef std::map<GURL, scoped_refptr<RefCountedVector<GURL> > > RedirectMap;

// Container for a list of URLs.
typedef std::vector<GURL> RedirectList;

typedef int64 DownloadID;   // Identifier for a download.
typedef int64 FaviconBitmapID; // Identifier for a bitmap in a favicon.
typedef int64 SegmentID;  // URL segments for the most visited view.

// The enumeration of all possible sources of visits is listed below.
// The source will be propagated along with a URL or a visit item
// and eventually be stored in the history database,
// visit_source table specifically.
// Different from page transition types, they describe the origins of visits.
// (Warning): Please don't change any existing values while it is ok to add
// new values when needed.
enum VisitSource {
  SOURCE_SYNCED = 0,         // Synchronized from somewhere else.
  SOURCE_BROWSED = 1,        // User browsed.
  SOURCE_EXTENSION = 2,      // Added by an extension.
  SOURCE_FIREFOX_IMPORTED = 3,
  SOURCE_IE_IMPORTED = 4,
  SOURCE_SAFARI_IMPORTED = 5,
};

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
           content::PageTransition arg_transition,
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
  content::PageTransition transition;

  // The segment id (see visitsegment_database.*).
  // If 0, the segment id is null in the table.
  SegmentID segment_id;

  // True when this visit has indexed data for it. We try to keep this in sync
  // with the full text index: when we add or remove things from there, we will
  // update the visit table as well. However, that file could get deleted, or
  // out of sync in various ways, so this flag should be false when things
  // change.
  bool is_indexed;

  // Record how much time a user has this visit starting from the user
  // opened this visit to the user closed or ended this visit.
  // This includes both active and inactive time as long as
  // the visit was present.
  base::TimeDelta visit_duration;

  // Compares two visits based on dates, for sorting.
  bool operator<(const VisitRow& other) {
    return visit_time < other.visit_time;
  }

  // We allow the implicit copy constuctor and operator=.
};

// We pass around vectors of visits a lot
typedef std::vector<VisitRow> VisitVector;

// The basic information associated with a visit (timestamp, type of visit),
// used by HistoryBackend::AddVisits() to create new visits for a URL.
typedef std::pair<base::Time, content::PageTransition> VisitInfo;

// PageVisit ------------------------------------------------------------------

// Represents a simplified version of a visit for external users. Normally,
// views are only interested in the time, and not the other information
// associated with a VisitRow.
struct PageVisit {
  URLID page_id;
  base::Time visit_time;
};

// URLResult -------------------------------------------------------------------

class URLResult : public URLRow {
 public:
  URLResult();
  URLResult(const GURL& url, base::Time visit_time);
  // Constructor that create a URLResult from the specified URL and title match
  // positions from title_matches.
  URLResult(const GURL& url, const Snippet::MatchPositions& title_matches);
  virtual ~URLResult();

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

  // Only search within the page body if true, otherwise search all columns
  // including url and time. Defaults to false.
  bool body_only;
};

// MostVisitedURL --------------------------------------------------------------

// Holds the per-URL information of the most visited query.
struct MostVisitedURL {
  MostVisitedURL();
  MostVisitedURL(const GURL& url, const string16& title);
  ~MostVisitedURL();

  GURL url;
  string16 title;

  RedirectList redirects;

  bool operator==(const MostVisitedURL& other) {
    return url == other.url;
  }
};

// FilteredURL -----------------------------------------------------------------

// Holds the per-URL information of the filterd url query.
struct FilteredURL {
  struct ExtendedInfo {
    ExtendedInfo();
    // The absolute number of visits.
    unsigned int total_visits;
    // The number of visits, as seen by the Most Visited NTP pane.
    unsigned int visits;
    // The total number of seconds that the page was open.
    int64 duration_opened;
    // The time when the page was last visited.
    base::Time last_visit_time;
  };

  FilteredURL();
  explicit FilteredURL(const PageUsageData& data);
  ~FilteredURL();

  GURL url;
  string16 title;
  double score;
  ExtendedInfo extended_info;
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
                     content::PageTransition arg_transition,
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
  content::PageTransition transition;
  VisitSource visit_source;
  bool did_replace_entry;

 private:
  friend class base::RefCountedThreadSafe<HistoryAddPageArgs>;

  ~HistoryAddPageArgs();

  DISALLOW_COPY_AND_ASSIGN(HistoryAddPageArgs);
};

// TopSites -------------------------------------------------------------------

typedef std::vector<MostVisitedURL> MostVisitedURLList;
typedef std::vector<FilteredURL> FilteredURLList;

// Used by TopSites to store the thumbnails.
struct Images {
  Images();
  ~Images();

  scoped_refptr<base::RefCountedBytes> thumbnail;
  ThumbnailScore thumbnail_score;

  // TODO(brettw): this will eventually store the favicon.
  // scoped_refptr<base::RefCountedBytes> favicon;
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

typedef std::map<GURL, scoped_refptr<base::RefCountedBytes> > URLToThumbnailMap;

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

  MostVisitedURLList most_visited;
  URLToImagesMap url_to_images_map;

 private:
  friend class base::RefCountedThreadSafe<MostVisitedThumbnails>;
  virtual ~MostVisitedThumbnails();

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

// Defines a favicon bitmap which best matches the desired DIP size and one of
// the desired scale factors.
struct FaviconBitmapResult {
  FaviconBitmapResult();
  ~FaviconBitmapResult();

  // Returns true if |bitmap_data| contains a valid bitmap.
  bool is_valid() const { return bitmap_data.get() && bitmap_data->size(); }

  // Indicates whether |bitmap_data| is expired.
  bool expired;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of |bitmap_data|.
  gfx::Size pixel_size;

  // The URL of the containing favicon.
  GURL icon_url;

  // The icon type of the containing favicon.
  IconType icon_type;
};

// Define type with same structure as FaviconBitmapResult for passing data to
// HistoryBackend::SetFavicons().
typedef FaviconBitmapResult FaviconBitmapData;

// Defines a gfx::Image of size desired_size_in_dip composed of image
// representations for each of the desired scale factors.
struct FaviconImageResult {
  FaviconImageResult();
  ~FaviconImageResult();

  // The resulting image.
  gfx::Image image;

  // The URL of the favicon which contains all of the image representations of
  // |image|.
  // TODO(pkotwicz): Return multiple |icon_urls| to allow |image| to have
  // representations from several favicons once content::FaviconStatus supports
  // multiple URLs.
  GURL icon_url;
};

// FaviconSizes represents the sizes that the thumbnail database knows a
// favicon is available from the web. FaviconSizes has several entries
// only if FaviconSizes is for an .ico file. FaviconSizes can be different
// from the pixel sizes of the entries in the |favicon_bitmaps| table. For
// instance, if a web page has a .ico favicon with bitmaps of pixel sizes
// (16x16, 32x32), FaviconSizes will have both sizes regardless of whether
// either of these bitmaps is cached in the favicon_bitmaps database table.
typedef std::vector<gfx::Size> FaviconSizes;

// Returns the default FaviconSizes to use if the favicon sizes for a FaviconID
// are unknown.
const FaviconSizes& GetDefaultFaviconSizes();

// A map from an icon URL to the FaviconSizes for that URL.
typedef std::map<GURL, FaviconSizes> IconURLSizesMap;

// Defines a favicon bitmap and its associated pixel size.
struct FaviconBitmapIDSize {
  FaviconBitmapIDSize();
  ~FaviconBitmapIDSize();

  // The unique id of the favicon bitmap.
  FaviconBitmapID bitmap_id;

  // The pixel dimensions of the associated bitmap.
  gfx::Size pixel_size;
};

// Defines a favicon bitmap stored in the history backend.
struct FaviconBitmap {
  FaviconBitmap();
  ~FaviconBitmap();

  // The unique id of the bitmap.
  FaviconBitmapID bitmap_id;

  // The id of the favicon to which the bitmap belongs to.
  FaviconID icon_id;

  // Time at which |bitmap_data| was last updated.
  base::Time last_updated;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of bitmap_data.
  gfx::Size pixel_size;
};

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

// Abbreviated information about a visit.
struct BriefVisitInfo {
  URLID url_id;
  base::Time time;
  content::PageTransition transition;
};

// An observer of VisitDatabase.
class VisitDatabaseObserver {
 public:
  virtual ~VisitDatabaseObserver() {}
  virtual void OnAddVisit(const BriefVisitInfo& info) = 0;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
