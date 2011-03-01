// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_BACKEND_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_BACKEND_H_
#pragma once

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "content/browser/cancelable_request.h"

class FilePath;

namespace history {

class TopSitesDatabase;

// Service used by TopSites to have db interaction happen on the DB thread.  All
// public methods are invoked on the ui thread and get funneled to the DB
// thread.
class TopSitesBackend
    : public base::RefCountedThreadSafe<TopSitesBackend>,
      public CancelableRequestProvider {
 public:
  TopSitesBackend();

  void Init(const FilePath& path);

  // Schedules the db to be shutdown.
  void Shutdown();

  // The boolean parameter indicates if the DB existed on disk or needs to be
  // migrated.
  typedef Callback3<Handle, scoped_refptr<MostVisitedThumbnails>, bool >::Type
      GetMostVisitedThumbnailsCallback;
  typedef CancelableRequest1<TopSitesBackend::GetMostVisitedThumbnailsCallback,
                             scoped_refptr<MostVisitedThumbnails> >
      GetMostVisitedThumbnailsRequest;

  // Fetches MostVisitedThumbnails.
  Handle GetMostVisitedThumbnails(CancelableRequestConsumerBase* consumer,
                                  GetMostVisitedThumbnailsCallback* callback);

  // Updates top sites database from the specified delta.
  void UpdateTopSites(const TopSitesDelta& delta);

  // Sets the thumbnail.
  void SetPageThumbnail(const MostVisitedURL& url,
                        int url_rank,
                        const Images& thumbnail);

  // Deletes the database and recreates it.
  void ResetDatabase();

  typedef Callback1<Handle>::Type EmptyRequestCallback;
  typedef CancelableRequest<TopSitesBackend::EmptyRequestCallback>
      EmptyRequestRequest;

  // Schedules a request that does nothing on the DB thread, but then notifies
  // the callback on the calling thread. This is used to make sure the db has
  // finished processing a request.
  Handle DoEmptyRequest(CancelableRequestConsumerBase* consumer,
                        EmptyRequestCallback* callback);

 private:
  friend class base::RefCountedThreadSafe<TopSitesBackend>;

  ~TopSitesBackend();

  // Invokes Init on the db_.
  void InitDBOnDBThread(const FilePath& path);

  // Shuts down the db.
  void ShutdownDBOnDBThread();

  // Does the work of getting the most visted thumbnails.
  void GetMostVisitedThumbnailsOnDBThread(
      scoped_refptr<GetMostVisitedThumbnailsRequest> request);

  // Updates top sites.
  void UpdateTopSitesOnDBThread(const TopSitesDelta& delta);

  // Sets the thumbnail.
  void SetPageThumbnailOnDBThread(const MostVisitedURL& url,
                                  int url_rank,
                                  const Images& thumbnail);

  // Resets the database.
  void ResetDatabaseOnDBThread(const FilePath& file_path);

  // Notifies the request.
  void DoEmptyRequestOnDBThread(scoped_refptr<EmptyRequestRequest> request);

  FilePath db_path_;

  scoped_ptr<TopSitesDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_BACKEND_H_
