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
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace safe_browsing {

namespace {

// Returns true if |first| is more recent than |second|, preferring opened over
// non-opened for downloads that completed at the same time (extraordinarily
// unlikely). Only files that look like some kind of executable are considered.
bool IsMoreInterestingThan(const history::DownloadRow& first,
                           const history::DownloadRow& second) {
  if (first.end_time < second.end_time)
    return false;
  // TODO(grt): Peek into archives to see if they contain binaries;
  // http://crbug.com/386915.
  if (!download_protection_util::IsBinaryFile(first.target_path) ||
      download_protection_util::IsArchiveFile(first.target_path)) {
    return false;
  }
  return (first.end_time != second.end_time ||
          (first.opened && !second.opened));
}

// Returns a pointer to the most interesting completed download in |downloads|.
const history::DownloadRow* FindMostInteresting(
    const std::vector<history::DownloadRow>& downloads) {
  const history::DownloadRow* most_recent_row = NULL;
  for (size_t i = 0; i < downloads.size(); ++i) {
    const history::DownloadRow& row = downloads[i];
    // Ignore incomplete downloads.
    if (row.state != content::DownloadItem::COMPLETE)
      continue;
    if (!most_recent_row || IsMoreInterestingThan(row, *most_recent_row))
      most_recent_row = &row;
  }
  return most_recent_row;
}

// Populates the |details| protobuf with information pertaining to |download|.
void PopulateDetails(const history::DownloadRow& download,
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
    const LastDownloadCallback& callback) {
  scoped_ptr<LastDownloadFinder> finder(make_scoped_ptr(new LastDownloadFinder(
      g_browser_process->profile_manager()->GetLoadedProfiles(), callback)));
  // Return NULL if there is no work to do.
  if (finder->profiles_.empty())
    return scoped_ptr<LastDownloadFinder>();
  return finder.Pass();
}

LastDownloadFinder::LastDownloadFinder() : weak_ptr_factory_(this) {
}

LastDownloadFinder::LastDownloadFinder(const std::vector<Profile*>& profiles,
                                       const LastDownloadCallback& callback)
    : callback_(callback), weak_ptr_factory_(this) {
  // Observe profile lifecycle events so that the finder can begin or abandon
  // the search in profiles while it is running.
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_ADDED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_HISTORY_LOADED,
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
  if (profile->IsOffTheRecord() ||
      !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return;
  }

  // Exit early if already processing this profile. This could happen if, for
  // example, NOTIFICATION_PROFILE_ADDED arrives after construction while
  // waiting for NOTIFICATION_HISTORY_LOADED.
  if (std::find(profiles_.begin(), profiles_.end(), profile) !=
      profiles_.end()) {
    return;
  }

  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile, Profile::IMPLICIT_ACCESS);
  // No history service is returned for profiles that do not save history.
  if (!history_service)
    return;

  profiles_.push_back(profile);
  if (history_service->BackendLoaded()) {
    history_service->QueryDownloads(
        base::Bind(&LastDownloadFinder::OnDownloadQuery,
                   weak_ptr_factory_.GetWeakPtr(),
                   profile));
  }  // else wait until history is loaded.
}

void LastDownloadFinder::OnProfileHistoryLoaded(
    Profile* profile,
    HistoryService* history_service) {
  if (std::find(profiles_.begin(), profiles_.end(), profile) !=
      profiles_.end()) {
    history_service->QueryDownloads(
        base::Bind(&LastDownloadFinder::OnDownloadQuery,
                   weak_ptr_factory_.GetWeakPtr(),
                   profile));
  }
}

void LastDownloadFinder::AbandonSearchInProfile(Profile* profile) {
  // |profile| may not be present in the set of profiles.
  std::vector<Profile*>::iterator it =
      std::find(profiles_.begin(), profiles_.end(), profile);
  if (it != profiles_.end())
    RemoveProfileAndReportIfDone(it);
}

void LastDownloadFinder::OnDownloadQuery(
    Profile* profile,
    scoped_ptr<std::vector<history::DownloadRow> > downloads) {
  // Early-exit if the history search for this profile was abandoned.
  std::vector<Profile*>::iterator it =
      std::find(profiles_.begin(), profiles_.end(), profile);
  if (it == profiles_.end())
    return;

  // Find the most recent from this profile and use it if it's better than
  // anything else found so far.
  const history::DownloadRow* profile_best = FindMostInteresting(*downloads);
  if (profile_best && IsMoreInterestingThan(*profile_best, most_recent_row_))
    most_recent_row_ = *profile_best;

  RemoveProfileAndReportIfDone(it);
}

void LastDownloadFinder::RemoveProfileAndReportIfDone(
    std::vector<Profile*>::iterator it) {
  DCHECK(it != profiles_.end());

  *it = profiles_.back();
  profiles_.resize(profiles_.size() - 1);

  // Finish processing if all results are in.
  if (profiles_.empty())
    ReportResults();
  // Do not touch this instance after reporting results.
}

void LastDownloadFinder::ReportResults() {
  DCHECK(profiles_.empty());
  if (most_recent_row_.end_time.is_null()) {
    callback_.Run(scoped_ptr<ClientIncidentReport_DownloadDetails>());
    // Do not touch this instance after running the callback, since it may have
    // been deleted.
  } else {
    scoped_ptr<ClientIncidentReport_DownloadDetails> details(
        new ClientIncidentReport_DownloadDetails());
    PopulateDetails(most_recent_row_, details.get());
    callback_.Run(details.Pass());
    // Do not touch this instance after running the callback, since it may have
    // been deleted.
  }
}

void LastDownloadFinder::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED:
      SearchInProfile(content::Source<Profile>(source).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_LOADED:
      OnProfileHistoryLoaded(content::Source<Profile>(source).ptr(),
                             content::Details<HistoryService>(details).ptr());
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      AbandonSearchInProfile(content::Source<Profile>(source).ptr());
      break;
    default:
      break;
  }
}

}  // namespace safe_browsing
