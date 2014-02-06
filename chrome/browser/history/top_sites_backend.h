// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_BACKEND_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_BACKEND_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/history/history_types.h"

namespace base {
class CancelableTaskTracker;
class FilePath;
}

namespace history {

class TopSitesDatabase;

// Service used by TopSites to have db interaction happen on the DB thread.  All
// public methods are invoked on the ui thread and get funneled to the DB
// thread.
class TopSitesBackend : public base::RefCountedThreadSafe<TopSitesBackend> {
 public:
  // The boolean parameter indicates if the DB existed on disk or needs to be
  // migrated.
  typedef base::Callback<void(const scoped_refptr<MostVisitedThumbnails>&)>
      GetMostVisitedThumbnailsCallback;

  TopSitesBackend();

  void Init(const base::FilePath& path);

  // Schedules the db to be shutdown.
  void Shutdown();

  // Fetches MostVisitedThumbnails.
  void GetMostVisitedThumbnails(
      const GetMostVisitedThumbnailsCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Updates top sites database from the specified delta.
  void UpdateTopSites(const TopSitesDelta& delta);

  // Sets the thumbnail.
  void SetPageThumbnail(const MostVisitedURL& url,
                        int url_rank,
                        const Images& thumbnail);

  // Deletes the database and recreates it.
  void ResetDatabase();

  // Schedules a request that does nothing on the DB thread, but then notifies
  // the the calling thread with a reply. This is used to make sure the db has
  // finished processing a request.
  void DoEmptyRequest(const base::Closure& reply,
                      base::CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<TopSitesBackend>;

  virtual ~TopSitesBackend();

  // Invokes Init on the db_.
  void InitDBOnDBThread(const base::FilePath& path);

  // Shuts down the db.
  void ShutdownDBOnDBThread();

  // Does the work of getting the most visted thumbnails.
  void GetMostVisitedThumbnailsOnDBThread(
      scoped_refptr<MostVisitedThumbnails> thumbnails);

  // Updates top sites.
  void UpdateTopSitesOnDBThread(const TopSitesDelta& delta);

  // Sets the thumbnail.
  void SetPageThumbnailOnDBThread(const MostVisitedURL& url,
                                  int url_rank,
                                  const Images& thumbnail);

  // Resets the database.
  void ResetDatabaseOnDBThread(const base::FilePath& file_path);

  base::FilePath db_path_;

  scoped_ptr<TopSitesDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_BACKEND_H_
