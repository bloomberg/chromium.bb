// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_LAST_DOWNLOAD_FINDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_LAST_DOWNLOAD_FINDER_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/history/download_row.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class HistoryService;
class Profile;

namespace content {
class NotificationDetails;
class NotificationSource;
}

namespace history {
struct DownloadRow;
}

namespace safe_browsing {

class ClientIncidentReport_DownloadDetails;

// Finds the most recent executable downloaded by any on-the-record profile with
// history that participates in safe browsing.
class LastDownloadFinder : public content::NotificationObserver {
 public:
  // The type of a callback run by the finder upon completion. The argument is a
  // protobuf containing details of the download that was found, or an empty
  // pointer if none was found.
  typedef base::Callback<void(scoped_ptr<ClientIncidentReport_DownloadDetails>)>
      LastDownloadCallback;

  virtual ~LastDownloadFinder();

  // Initiates an asynchronous search for the most recent download. |callback|
  // will be run when the search is complete. The returned instance can be
  // deleted to terminate the search, in which case |callback| is not invoked.
  // Returns NULL without running |callback| if there are no eligible profiles
  // to search.
  static scoped_ptr<LastDownloadFinder> Create(
      const LastDownloadCallback& callback);

 protected:
  // Protected constructor so that unit tests can create a fake finder.
  LastDownloadFinder();

 private:
  LastDownloadFinder(const std::vector<Profile*>& profiles,
                     const LastDownloadCallback& callback);

  // Adds |profile| to the set of profiles to be searched if it is an
  // on-the-record profile with history that participates in safe browsing. The
  // search is initiated if the profile has already loaded history.
  void SearchInProfile(Profile* profile);

  // Initiates a search in |profile| if it is in the set of profiles to be
  // searched.
  void OnProfileHistoryLoaded(Profile* profile,
                              HistoryService* history_service);

  // Abandons the search for downloads in |profile|, reporting results if there
  // are no more pending queries.
  void AbandonSearchInProfile(Profile* profile);

  // HistoryService::DownloadQueryCallback. Retrieves the most recent completed
  // executable download from |downloads| and reports results if there are no
  // more pending queries.
  void OnDownloadQuery(
      Profile* profile,
      scoped_ptr<std::vector<history::DownloadRow> > downloads);

  // Removes the profile pointed to by |it| from profiles_ and reports results
  // if there are no more pending queries.
  void RemoveProfileAndReportIfDone(std::vector<Profile*>::iterator it);

  // Invokes the caller-supplied callback with the download found.
  void ReportResults();

  // content::NotificationObserver methods.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Caller-supplied callback to be invoked when the most recent download is
  // found.
  LastDownloadCallback callback_;

  // The profiles for which a download query is pending.
  std::vector<Profile*> profiles_;

  // Registrar for observing profile lifecycle notifications.
  content::NotificationRegistrar notification_registrar_;

  // The most recent download, updated progressively as query results arrive.
  history::DownloadRow most_recent_row_;

  // A factory for asynchronous operations on profiles' HistoryService.
  base::WeakPtrFactory<LastDownloadFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LastDownloadFinder);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_LAST_DOWNLOAD_FINDER_H_
