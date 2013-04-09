// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_feature_extractor.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_features.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace safe_browsing {

BrowseInfo::BrowseInfo() : http_status_code(0) {}

BrowseInfo::~BrowseInfo() {}

static void AddFeature(const std::string& feature_name,
                       double feature_value,
                       ClientPhishingRequest* request) {
  DCHECK(request);
  ClientPhishingRequest::Feature* feature =
      request->add_non_model_feature_map();
  feature->set_name(feature_name);
  feature->set_value(feature_value);
  VLOG(2) << "Browser feature: " << feature->name() << " " << feature->value();
}

static void AddMalwareFeature(const std::string& feature_name,
                              const std::set<std::string>& meta_infos,
                              double feature_value,
                              ClientMalwareRequest* request) {
  DCHECK(request);
  ClientMalwareRequest::Feature* feature =
      request->add_feature_map();
  feature->set_name(feature_name);
  feature->set_value(feature_value);
  for (std::set<std::string>::const_iterator it = meta_infos.begin();
       it != meta_infos.end(); ++it) {
    feature->add_metainfo(*it);
  }
  VLOG(2) << "Browser feature: " << feature->name() << " " << feature->value();
}

static void AddNavigationFeatures(
    const std::string& feature_prefix,
    const NavigationController& controller,
    int index,
    const std::vector<GURL>& redirect_chain,
    ClientPhishingRequest* request) {
  NavigationEntry* entry = controller.GetEntryAtIndex(index);
  bool is_secure_referrer = entry->GetReferrer().url.SchemeIsSecure();
  if (!is_secure_referrer) {
    AddFeature(base::StringPrintf("%s%s=%s",
                                  feature_prefix.c_str(),
                                  features::kReferrer,
                                  entry->GetReferrer().url.spec().c_str()),
               1.0,
               request);
  }
  AddFeature(feature_prefix + features::kHasSSLReferrer,
             is_secure_referrer ? 1.0 : 0.0,
             request);
  AddFeature(feature_prefix + features::kPageTransitionType,
             static_cast<double>(
                 content::PageTransitionStripQualifier(
                    entry->GetTransitionType())),
             request);
  AddFeature(feature_prefix + features::kIsFirstNavigation,
             index == 0 ? 1.0 : 0.0,
             request);
  // Redirect chain should always be at least of size one, as the rendered
  // url is the last element in the chain.
  if (redirect_chain.empty()) {
    NOTREACHED();
    return;
  }
  if (redirect_chain.back() != entry->GetURL()) {
    // I originally had this as a DCHECK but I saw a failure once that I
    // can't reproduce. It looks like it might be related to the
    // navigation controller only keeping a limited number of navigation
    // events. For now we'll just attach a feature specifying that this is
    // a mismatch and try and figure out what to do with it on the server.
    DLOG(WARNING) << "Expected:" << entry->GetURL()
                 << " Actual:" << redirect_chain.back();
    AddFeature(feature_prefix + features::kRedirectUrlMismatch,
               1.0,
               request);
    return;
  }
  // We skip the last element since it should just be the current url.
  for (size_t i = 0; i < redirect_chain.size() - 1; i++) {
    std::string printable_redirect = redirect_chain[i].spec();
    if (redirect_chain[i].SchemeIsSecure()) {
      printable_redirect = features::kSecureRedirectValue;
    }
    AddFeature(base::StringPrintf("%s%s[%"PRIuS"]=%s",
                                  feature_prefix.c_str(),
                                  features::kRedirect,
                                  i,
                                  printable_redirect.c_str()),
               1.0,
               request);
  }
}

BrowserFeatureExtractor::BrowserFeatureExtractor(
    WebContents* tab,
    ClientSideDetectionService* service)
    : tab_(tab),
      service_(service),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(tab);
}

BrowserFeatureExtractor::~BrowserFeatureExtractor() {
  weak_factory_.InvalidateWeakPtrs();
  // Delete all the pending extractions (delete callback and request objects).
  STLDeleteContainerPairFirstPointers(pending_extractions_.begin(),
                                      pending_extractions_.end());

  // Also cancel all the pending history service queries.
  HistoryService* history;
  bool success = GetHistoryService(&history);
  DCHECK(success || pending_queries_.size() == 0);
  // Cancel all the pending history lookups and cleanup the memory.
  for (PendingQueriesMap::iterator it = pending_queries_.begin();
       it != pending_queries_.end(); ++it) {
    if (history) {
      history->CancelRequest(it->first);
    }
    ExtractionData& extraction = it->second;
    delete extraction.first;  // delete request
  }
  pending_queries_.clear();
}

void BrowserFeatureExtractor::ExtractFeatures(const BrowseInfo* info,
                                              ClientPhishingRequest* request,
                                              const DoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(request);
  DCHECK(info);
  DCHECK_EQ(0U, request->url().find("http:"));
  DCHECK(!callback.is_null());
  if (callback.is_null()) {
    DLOG(ERROR) << "ExtractFeatures called without a callback object";
    return;
  }

  // Extract features pertaining to this navigation.
  const NavigationController& controller = tab_->GetController();
  int url_index = -1;
  int first_host_index = -1;

  GURL request_url(request->url());
  int index = controller.GetCurrentEntryIndex();
  // The url that we are extracting features for should already be commited.
  DCHECK_NE(index, -1);
  for (; index >= 0; index--) {
    NavigationEntry* entry = controller.GetEntryAtIndex(index);
    if (url_index == -1 && entry->GetURL() == request_url) {
      // It's possible that we've been on the on the possibly phishy url before
      // in this tab, so make sure that we use the latest navigation for
      // features.
      // Note that it's possible that the url_index should always be the
      // latest entry, but I'm worried about possible races during a navigation
      // and transient entries (i.e. interstiatials) so for now we will just
      // be cautious.
      url_index = index;
    } else if (index < url_index) {
      if (entry->GetURL().host() == request_url.host()) {
        first_host_index = index;
      } else {
        // We have found the possibly phishing url, but we are no longer on the
        // host. No reason to look back any further.
        break;
      }
    }
  }

  // Add features pertaining to how we got to
  //   1) The candidate url
  //   2) The first url on the same host as the candidate url (assuming that
  //      it's different from the candidate url).
  if (url_index != -1) {
    AddNavigationFeatures("", controller, url_index, info->url_redirects,
                          request);
  }
  if (first_host_index != -1) {
    AddNavigationFeatures(features::kHostPrefix,
                          controller,
                          first_host_index,
                          info->host_redirects,
                          request);
  }

  ExtractBrowseInfoFeatures(*info, request);
  pending_extractions_[request] = callback;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserFeatureExtractor::StartExtractFeatures,
                 weak_factory_.GetWeakPtr(), request, callback));
}

void BrowserFeatureExtractor::ExtractMalwareFeatures(
    const BrowseInfo* info,
    ClientMalwareRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(request);
  DCHECK(info);
  DCHECK_EQ(0U, request->url().find("http:"));
  // get the IPs and hosts that match the malware blacklisted IP list.
  if (service_) {
    for (IPHostMap::const_iterator it = info->ips.begin();
         it != info->ips.end(); ++it) {
      if (service_->IsBadIpAddress(it->first)) {
        AddMalwareFeature(features::kBadIpFetch + it->first,
                          it->second, 1.0, request);
      }
    }
  }
}

void BrowserFeatureExtractor::ExtractBrowseInfoFeatures(
    const BrowseInfo& info,
    ClientPhishingRequest* request) {
  if (service_) {
    for (IPHostMap::const_iterator it = info.ips.begin();
         it != info.ips.end(); ++it) {
      if (service_->IsBadIpAddress(it->first)) {
        AddFeature(features::kBadIpFetch + it->first, 1.0, request);
      }
    }
  }
  if (info.unsafe_resource.get()) {
    // A SafeBrowsing interstitial was shown for the current URL.
    AddFeature(features::kSafeBrowsingMaliciousUrl +
               info.unsafe_resource->url.spec(),
               1.0,
               request);
    AddFeature(features::kSafeBrowsingOriginalUrl +
               info.unsafe_resource->original_url.spec(),
               1.0,
               request);
    AddFeature(features::kSafeBrowsingIsSubresource,
               info.unsafe_resource->is_subresource ? 1.0 : 0.0,
               request);
    AddFeature(features::kSafeBrowsingThreatType,
               static_cast<double>(info.unsafe_resource->threat_type),
               request);
  }
  if (info.http_status_code != 0) {
    AddFeature(features::kHttpStatusCode, info.http_status_code, request);
  }
}

void BrowserFeatureExtractor::StartExtractFeatures(
    ClientPhishingRequest* request,
    const DoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t removed = pending_extractions_.erase(request);
  DCHECK_EQ(1U, removed);
  HistoryService* history;
  if (!request || !request->IsInitialized() || !GetHistoryService(&history)) {
    callback.Run(false, request);
    return;
  }
  CancelableRequestProvider::Handle handle = history->QueryURL(
      GURL(request->url()),
      true /* wants_visits */,
      &request_consumer_,
      base::Bind(&BrowserFeatureExtractor::QueryUrlHistoryDone,
                 base::Unretained(this)));

  StorePendingQuery(handle, request, callback);
}

void BrowserFeatureExtractor::QueryUrlHistoryDone(
    CancelableRequestProvider::Handle handle,
    bool success,
    const history::URLRow* row,
    history::VisitVector* visits) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ClientPhishingRequest* request;
  DoneCallback callback;
  if (!GetPendingQuery(handle, &request, &callback)) {
    DLOG(FATAL) << "No pending history query found";
    return;
  }
  DCHECK(request);
  DCHECK(!callback.is_null());
  if (!success) {
    // URL is not found in the history.  In practice this should not
    // happen (unless there is a real error) because we just visited
    // that URL.
    callback.Run(false, request);
    return;
  }
  AddFeature(features::kUrlHistoryVisitCount,
             static_cast<double>(row->visit_count()),
             request);

  base::Time threshold = base::Time::Now() - base::TimeDelta::FromDays(1);
  int num_visits_24h_ago = 0;
  int num_visits_typed = 0;
  int num_visits_link = 0;
  for (history::VisitVector::const_iterator it = visits->begin();
       it != visits->end(); ++it) {
    if (!content::PageTransitionIsMainFrame(it->transition)) {
      continue;
    }
    if (it->visit_time < threshold) {
      ++num_visits_24h_ago;
    }
    content::PageTransition transition = content::PageTransitionStripQualifier(
        it->transition);
    if (transition == content::PAGE_TRANSITION_TYPED) {
      ++num_visits_typed;
    } else if (transition == content::PAGE_TRANSITION_LINK) {
      ++num_visits_link;
    }
  }
  AddFeature(features::kUrlHistoryVisitCountMoreThan24hAgo,
             static_cast<double>(num_visits_24h_ago),
             request);
  AddFeature(features::kUrlHistoryTypedCount,
             static_cast<double>(num_visits_typed),
             request);
  AddFeature(features::kUrlHistoryLinkCount,
             static_cast<double>(num_visits_link),
             request);

  // Issue next history lookup for host visits.
  HistoryService* history;
  if (!GetHistoryService(&history)) {
    callback.Run(false, request);
    return;
  }
  CancelableRequestProvider::Handle next_handle =
      history->GetVisibleVisitCountToHost(
          GURL(request->url()),
          &request_consumer_,
          base::Bind(&BrowserFeatureExtractor::QueryHttpHostVisitsDone,
                     base::Unretained(this)));
  StorePendingQuery(next_handle, request, callback);
}

void BrowserFeatureExtractor::QueryHttpHostVisitsDone(
    CancelableRequestProvider::Handle handle,
    bool success,
    int num_visits,
    base::Time first_visit) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ClientPhishingRequest* request;
  DoneCallback callback;
  if (!GetPendingQuery(handle, &request, &callback)) {
    DLOG(FATAL) << "No pending history query found";
    return;
  }
  DCHECK(request);
  DCHECK(!callback.is_null());
  if (!success) {
    callback.Run(false, request);
    return;
  }
  SetHostVisitsFeatures(num_visits, first_visit, true, request);

  // Same lookup but for the HTTPS URL.
  HistoryService* history;
  if (!GetHistoryService(&history)) {
    callback.Run(false, request);
    return;
  }
  std::string https_url = request->url();
  CancelableRequestProvider::Handle next_handle =
      history->GetVisibleVisitCountToHost(
          GURL(https_url.replace(0, 5, "https:")),
          &request_consumer_,
          base::Bind(&BrowserFeatureExtractor::QueryHttpsHostVisitsDone,
                     base::Unretained(this)));
  StorePendingQuery(next_handle, request, callback);
}

void BrowserFeatureExtractor::QueryHttpsHostVisitsDone(
    CancelableRequestProvider::Handle handle,
    bool success,
    int num_visits,
    base::Time first_visit) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ClientPhishingRequest* request;
  DoneCallback callback;
  if (!GetPendingQuery(handle, &request, &callback)) {
    DLOG(FATAL) << "No pending history query found";
    return;
  }
  DCHECK(request);
  DCHECK(!callback.is_null());
  if (!success) {
    callback.Run(false, request);
    return;
  }
  SetHostVisitsFeatures(num_visits, first_visit, false, request);
  callback.Run(true, request);  // We're done with all the history lookups.
}

void BrowserFeatureExtractor::SetHostVisitsFeatures(
    int num_visits,
    base::Time first_visit,
    bool is_http_query,
    ClientPhishingRequest* request) {
  DCHECK(request);
  AddFeature(is_http_query ?
             features::kHttpHostVisitCount : features::kHttpsHostVisitCount,
             static_cast<double>(num_visits),
             request);
  if (num_visits > 0) {
    AddFeature(
        is_http_query ?
        features::kFirstHttpHostVisitMoreThan24hAgo :
        features::kFirstHttpsHostVisitMoreThan24hAgo,
        (first_visit < (base::Time::Now() - base::TimeDelta::FromDays(1))) ?
        1.0 : 0.0,
        request);
  }
}

void BrowserFeatureExtractor::StorePendingQuery(
    CancelableRequestProvider::Handle handle,
    ClientPhishingRequest* request,
    const DoneCallback& callback) {
  DCHECK_EQ(0U, pending_queries_.count(handle));
  pending_queries_[handle] = std::make_pair(request, callback);
}

bool BrowserFeatureExtractor::GetPendingQuery(
    CancelableRequestProvider::Handle handle,
    ClientPhishingRequest** request,
    DoneCallback* callback) {
  PendingQueriesMap::iterator it = pending_queries_.find(handle);
  DCHECK(it != pending_queries_.end());
  if (it != pending_queries_.end()) {
    *request = it->second.first;
    *callback = it->second.second;
    pending_queries_.erase(it);
    return true;
  }
  return false;
}

bool BrowserFeatureExtractor::GetHistoryService(HistoryService** history) {
  *history = NULL;
  if (tab_ && tab_->GetBrowserContext()) {
    Profile* profile = Profile::FromBrowserContext(tab_->GetBrowserContext());
    *history = HistoryServiceFactory::GetForProfile(profile,
                                                    Profile::EXPLICIT_ACCESS);
    if (*history) {
      return true;
    }
  }
  VLOG(2) << "Unable to query history.  No history service available.";
  return false;
}

}  // namespace safe_browsing
