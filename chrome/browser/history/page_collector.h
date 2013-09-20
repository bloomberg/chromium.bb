// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_PAGE_COLLECTOR_H_
#define CHROME_BROWSER_HISTORY_PAGE_COLLECTOR_H_

#include "base/basictypes.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

class GURL;

namespace history {

class HistoryPublisher;

// Collect page data and publish to HistoryPublisher.
class PageCollector {
 public:
  // You must call Init() to complete initialization.
  PageCollector();
  ~PageCollector();

  // Must call before using other functions.
  void Init(const HistoryPublisher* history_publisher);

  // Sets specific information for the given page to be published.
  // In normal operation, URLs will be added as the user visits them, the titles
  // and bodies will come in some time after that. These changes will be
  // automatically coalesced and added to the database some time in the future
  // using AddPageData().
  //
  // AddPageURL must be called for a given URL before either the title
  // or body set. The visit time should be the time corresponding to
  // that visit in the history database.
  void AddPageURL(const GURL& url, base::Time visit_time);
  void AddPageTitle(const GURL& url, const string16& title);
  void AddPageContents(const GURL& url, const string16& body);

  void AddPageData(const GURL& url,
                   base::Time visit_time,
                   const string16& title,
                   const string16& body);

 private:
  // Stores "recent stuff" that has happened with the page, since the page
  // visit, title, and body all come in at different times.
  class PageInfo {
   public:
    explicit PageInfo(base::Time visit_time);
    ~PageInfo();

    // Getters.
    base::Time visit_time() const { return visit_time_; }
    const string16& title() const { return title_; }
    const string16& body() const { return body_; }

    // Setters, we can only update the title and body.
    void set_title(const string16& ttl);
    void set_body(const string16& bdy);

    // Returns true if both the title or body of the entry has been set. Since
    // both the title and body setters will "fix" empty strings to be a space,
    // these indicate if the setter was ever called.
    bool has_title() const { return !title_.empty(); }
    bool has_body() const { return !body_.empty(); }

    // Returns true if this entry was added too long ago and we should give up
    // waiting for more data. The current time is passed in as an argument so we
    // can check many without re-querying the timer.
    bool Expired(base::TimeTicks now) const;

   private:
    // Time of the visit of the URL. This will be the value stored in the URL
    // and visit tables for the entry.
    base::Time visit_time_;

    // When this page entry was created. We have a cap on the maximum time that
    // an entry will be in the queue before being flushed to the database.
    base::TimeTicks added_time_;

    // Will be the string " " when they are set to distinguish set and unset.
    string16 title_;
    string16 body_;
  };

  // Collected data is published when both the title and body are
  // present.  https data is never passed to AddPageContents(), so
  // periodically collected data is published without the contents.
  // Pages which take a long time to load will not have their bodies
  // published.
  void ScheduleFlushCollected();
  void FlushCollected();

  // Lists recent additions that we have not yet filled out with the title and
  // body. Sorted by time, we will flush them when they are complete or have
  // been in the queue too long without modification.
  //
  // We kind of abuse the MRUCache because we never move things around in it
  // using Get. Instead, we keep them in the order they were inserted, since
  // this is the metric we use to measure age. The MRUCache gives us an ordered
  // list with fast lookup by URL.
  typedef base::MRUCache<GURL, PageInfo> RecentChangeList;
  RecentChangeList recent_changes_;

  // Generates tasks for our periodic checking of expired "recent changes".
  base::WeakPtrFactory<PageCollector> weak_factory_;

  // This object is created and managed by the history backend. We maintain an
  // opaque pointer to the object for our use.
  // This can be NULL if there are no indexers registered to receive indexing
  // data from us.
  const HistoryPublisher* history_publisher_;

  DISALLOW_COPY_AND_ASSIGN(PageCollector);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_PAGE_COLLECTOR_H_
