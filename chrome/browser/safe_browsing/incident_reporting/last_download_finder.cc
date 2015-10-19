// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace safe_browsing {

namespace {

// The following functions are overloaded for the two object types that
// represent the metadata for a download: history::DownloadRow and
// ClientIncidentReport_DownloadDetails. These are used by the template
// functions that follow.

// Returns the end time of a download represented by a DownloadRow.
int64 GetEndTime(const history::DownloadRow& row) {
  return row.end_time.ToJavaTime();
}

// Returns the end time of a download represented by a DownloadDetails.
int64 GetEndTime(const ClientIncidentReport_DownloadDetails& details) {
  return details.download_time_msec();
}

bool IsBinaryDownloadForCurrentOS(
    ClientDownloadRequest::DownloadType download_type) {
  static_assert(ClientDownloadRequest::DownloadType_MAX ==
                    ClientDownloadRequest::ARCHIVE,
                "Update logic below");

// Platform-specific types are relevant only for their own platforms.
#if defined(OS_MACOSX)
  if (download_type == ClientDownloadRequest::MAC_EXECUTABLE)
    return true;
#elif defined(OS_ANDROID)
  if (download_type == ClientDownloadRequest::ANDROID_APK)
    return true;
#endif

// Extensions are supported where enabled.
#if defined(ENABLE_EXTENSIONS)
  if (download_type == ClientDownloadRequest::CHROME_EXTENSION)
    return true;
#endif

  if (download_type == ClientDownloadRequest::ZIPPED_EXECUTABLE ||
      download_type == ClientDownloadRequest::ZIPPED_ARCHIVE ||
      download_type == ClientDownloadRequest::ARCHIVE) {
    return true;
  }

  // The default return value of download_protection_util::GetDownloadType is
  // ClientDownloadRequest::WIN_EXECUTABLE.
  return download_type == ClientDownloadRequest::WIN_EXECUTABLE;
}

// Returns true if a download represented by a DownloadRow is binary file for
// the current OS.
bool IsBinaryDownload(const history::DownloadRow& row) {
  // TODO(grt): Peek into archives to see if they contain binaries;
  // http://crbug.com/386915.
  return (download_protection_util::IsSupportedBinaryFile(row.target_path) &&
          !download_protection_util::IsArchiveFile(row.target_path) &&
          IsBinaryDownloadForCurrentOS(
              download_protection_util::GetDownloadType(row.target_path)));
}

// Returns true if a download represented by a DownloadDetails is binary file
// for the current OS.
bool IsBinaryDownload(const ClientIncidentReport_DownloadDetails& details) {
  // DownloadDetails are only generated for binary downloads.
  return IsBinaryDownloadForCurrentOS(details.download().download_type());
}

// Returns true if a download represented by a DownloadRow has been opened.
bool HasBeenOpened(const history::DownloadRow& row) {
  return row.opened;
}

// Returns true if a download represented by a DownloadDetails has been opened.
bool HasBeenOpened(const ClientIncidentReport_DownloadDetails& details) {
  return details.has_open_time_msec() && details.open_time_msec();
}

// Returns true if |first| is more recent than |second|, preferring opened over
// non-opened for downloads that completed at the same time (extraordinarily
// unlikely). Only files that look like some kind of executable are considered.
template <class A, class B>
bool IsMoreInterestingThan(const A& first, const B& second) {
  if (GetEndTime(first) < GetEndTime(second) || !IsBinaryDownload(first))
    return false;
  return (GetEndTime(first) != GetEndTime(second) ||
          (HasBeenOpened(first) && !HasBeenOpened(second)));
}

// Returns a pointer to the most interesting completed download in |downloads|.
const history::DownloadRow* FindMostInteresting(
    const std::vector<history::DownloadRow>& downloads) {
  const history::DownloadRow* most_recent_row = NULL;
  for (size_t i = 0; i < downloads.size(); ++i) {
    const history::DownloadRow& row = downloads[i];
    // Ignore incomplete downloads.
    if (row.state != history::DownloadState::COMPLETE)
      continue;
    if (!most_recent_row || IsMoreInterestingThan(row, *most_recent_row))
      most_recent_row = &row;
  }
  return most_recent_row;
}

// Returns true if |candidate| is more interesting than whichever of |details|
// or |best_row| is present.
template <class D>
bool IsMostInteresting(const D& candidate,
                       const ClientIncidentReport_DownloadDetails* details,
                       const history::DownloadRow& best_row) {
  return details ?
      IsMoreInterestingThan(candidate, *details) :
      IsMoreInterestingThan(candidate, best_row);
}

// Populates the |details| protobuf with information pertaining to |download|.
void PopulateDetailsFromRow(const history::DownloadRow& download,
                            ClientIncidentReport_DownloadDetails* details) {
  ClientDownloadRequest* download_request = details->mutable_download();
  download_request->set_url(download.url_chain.back().spec());
  // digests is a required field, so force it to exist.
  // TODO(grt): Include digests in reports; http://crbug.com/389123.
  ignore_result(download_request->mutable_digests());
  download_request->set_length(download.received_bytes);
  for (size_t i = 0; i < download.url_chain.size(); ++i) {
    const GURL& url = download.url_chain[i];
    ClientDownloadRequest_Resource* resource =
        download_request->add_resources();
    resource->set_url(url.spec());
    if (i != download.url_chain.size() - 1) {  // An intermediate redirect.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    } else {  // The final download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      if (!download.referrer_url.is_empty())
        resource->set_referrer(download.referrer_url.spec());
    }
  }
  download_request->set_file_basename(
      download.target_path.BaseName().AsUTF8Unsafe());
  download_request->set_download_type(
      download_protection_util::GetDownloadType(download.target_path));
  download_request->set_locale(
      g_browser_process->local_state()->GetString(prefs::kApplicationLocale));

  details->set_download_time_msec(download.end_time.ToJavaTime());
  // Opened time is unknown for now, so use the download time if the file was
  // opened in Chrome.
  if (download.opened)
    details->set_open_time_msec(download.end_time.ToJavaTime());
}

}  // namespace

LastDownloadFinder::~LastDownloadFinder() {
}

// static
scoped_ptr<LastDownloadFinder> LastDownloadFinder::Create(
    const DownloadDetailsGetter& download_details_getter,
    const LastDownloadCallback& callback) {
  scoped_ptr<LastDownloadFinder> finder(make_scoped_ptr(new LastDownloadFinder(
      download_details_getter,
      g_browser_process->profile_manager()->GetLoadedProfiles(),
      callback)));
  // Return NULL if there is no work to do.
  if (finder->profile_states_.empty())
    return scoped_ptr<LastDownloadFinder>();
  return finder.Pass();
}

LastDownloadFinder::LastDownloadFinder()
    : history_service_observer_(this), weak_ptr_factory_(this) {
}

LastDownloadFinder::LastDownloadFinder(
    const DownloadDetailsGetter& download_details_getter,
    const std::vector<Profile*>& profiles,
    const LastDownloadCallback& callback)
    : download_details_getter_(download_details_getter),
      callback_(callback),
      history_service_observer_(this),
      weak_ptr_factory_(this) {
  // Observe profile lifecycle events so that the finder can begin or abandon
  // the search in profiles while it is running.
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_ADDED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());

  // Begin the seach for all given profiles.
  std::for_each(
      profiles.begin(),
      profiles.end(),
      std::bind1st(std::mem_fun(&LastDownloadFinder::SearchInProfile), this));
}

void LastDownloadFinder::SearchInProfile(Profile* profile) {
  // Do not look in OTR profiles or in profiles that do not participate in
  // safe browsing.
  if (!IncidentReportingService::IsEnabledForProfile(profile))
    return;

  // Exit early if already processing this profile. This could happen if, for
  // example, NOTIFICATION_PROFILE_ADDED arrives after construction while
  // waiting for OnHistoryServiceLoaded.
  if (profile_states_.count(profile))
    return;

  // Initiate a metadata search.
  profile_states_[profile] = WAITING_FOR_METADATA;
  download_details_getter_.Run(profile,
                               base::Bind(&LastDownloadFinder::OnMetadataQuery,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          profile));
}

void LastDownloadFinder::OnMetadataQuery(
    Profile* profile,
    scoped_ptr<ClientIncidentReport_DownloadDetails> details) {
  auto iter = profile_states_.find(profile);
  // Early-exit if the search for this profile was abandoned.
  if (iter == profile_states_.end())
    return;

  if (details) {
    if (IsMostInteresting(*details, details_.get(), most_recent_row_)) {
      details_ = details.Pass();
      most_recent_row_.end_time = base::Time();
    }

    RemoveProfileAndReportIfDone(iter);
  } else {
    // Search history since no metadata was found.
    iter->second = WAITING_FOR_HISTORY;
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS);
    // No history service is returned for profiles that do not save history.
    if (!history_service) {
      RemoveProfileAndReportIfDone(iter);
      return;
    }
    if (history_service->BackendLoaded()) {
      history_service->QueryDownloads(
          base::Bind(&LastDownloadFinder::OnDownloadQuery,
                     weak_ptr_factory_.GetWeakPtr(),
                     profile));
    } else {
      // else wait until history is loaded.
      history_service_observer_.Add(history_service);
    }
  }
}

void LastDownloadFinder::AbandonSearchInProfile(Profile* profile) {
  // |profile| may not be present in the set of profiles.
  auto iter = profile_states_.find(profile);
  if (iter != profile_states_.end())
    RemoveProfileAndReportIfDone(iter);
}

void LastDownloadFinder::OnDownloadQuery(
    Profile* profile,
    scoped_ptr<std::vector<history::DownloadRow> > downloads) {
  // Early-exit if the history search for this profile was abandoned.
  auto iter = profile_states_.find(profile);
  if (iter == profile_states_.end())
    return;

  // Find the most recent from this profile and use it if it's better than
  // anything else found so far.
  const history::DownloadRow* profile_best = FindMostInteresting(*downloads);
  if (profile_best &&
      IsMostInteresting(*profile_best, details_.get(), most_recent_row_)) {
    details_.reset();
    most_recent_row_ = *profile_best;
  }

  RemoveProfileAndReportIfDone(iter);
}

void LastDownloadFinder::RemoveProfileAndReportIfDone(
    std::map<Profile*, ProfileWaitState>::iterator iter) {
  DCHECK(iter != profile_states_.end());
  profile_states_.erase(iter);

  // Finish processing if all results are in.
  if (profile_states_.empty())
    ReportResults();
  // Do not touch this LastDownloadFinder after reporting results.
}

void LastDownloadFinder::ReportResults() {
  DCHECK(profile_states_.empty());
  if (details_) {
    callback_.Run(make_scoped_ptr(new ClientIncidentReport_DownloadDetails(
                                      *details_)).Pass());
    // Do not touch this LastDownloadFinder after running the callback, since it
    // may have been deleted.
  } else if (!most_recent_row_.end_time.is_null()) {
    scoped_ptr<ClientIncidentReport_DownloadDetails> details(
        new ClientIncidentReport_DownloadDetails());
    PopulateDetailsFromRow(most_recent_row_, details.get());
    callback_.Run(details.Pass());
    // Do not touch this LastDownloadFinder after running the callback, since it
    // may have been deleted.
  } else {
    callback_.Run(scoped_ptr<ClientIncidentReport_DownloadDetails>());
    // Do not touch this LastDownloadFinder after running the callback, since it
    // may have been deleted.
  }
}

void LastDownloadFinder::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED:
      SearchInProfile(content::Source<Profile>(source).ptr());
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      AbandonSearchInProfile(content::Source<Profile>(source).ptr());
      break;
    default:
      break;
  }
}

void LastDownloadFinder::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  for (const auto& pair : profile_states_) {
    history::HistoryService* hs = HistoryServiceFactory::GetForProfileIfExists(
        pair.first, ServiceAccessType::EXPLICIT_ACCESS);
    if (hs == history_service) {
      // Start the query in the history service if the finder was waiting for
      // the service to load.
      if (pair.second == WAITING_FOR_HISTORY) {
        history_service->QueryDownloads(
            base::Bind(&LastDownloadFinder::OnDownloadQuery,
                       weak_ptr_factory_.GetWeakPtr(),
                       pair.first));
      }
      return;
    }
  }
}

void LastDownloadFinder::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.Remove(history_service);
}

}  // namespace safe_browsing
