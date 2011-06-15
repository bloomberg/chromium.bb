// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_feature_extractor.h"

#include <map>
#include <utility>

#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/page_transition_types.h"
#include "content/browser/browser_thread.h"
#include "content/browser/cancelable_request.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace safe_browsing {
namespace features {
const char kUrlHistoryVisitCount[] = "UrlHistoryVisitCount";
const char kUrlHistoryTypedCount[] = "UrlHistoryTypedCount";
const char kUrlHistoryLinkCount[] = "UrlHistoryLinkCount";
const char kUrlHistoryVisitCountMoreThan24hAgo[] =
    "UrlHistoryVisitCountMoreThan24hAgo";
const char kHttpHostVisitCount[] = "HttpHostVisitCount";
const char kHttpsHostVisitCount[] = "HttpsHostVisitCount";
const char kFirstHttpHostVisitMoreThan24hAgo[] =
    "FirstHttpHostVisitMoreThan24hAgo";
const char kFirstHttpsHostVisitMoreThan24hAgo[] =
    "FirstHttpsHostVisitMoreThan24hAgo";
}  // namespace features

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

BrowserFeatureExtractor::BrowserFeatureExtractor(TabContents* tab)
    : tab_(tab),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(tab);
}

BrowserFeatureExtractor::~BrowserFeatureExtractor() {
  method_factory_.RevokeAll();
  // Delete all the pending extractions (delete callback and request objects).
  STLDeleteContainerPairPointers(pending_extractions_.begin(),
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
    delete extraction.second;  // delete callback
  }
  pending_queries_.clear();
}

void BrowserFeatureExtractor::ExtractFeatures(ClientPhishingRequest* request,
                                              DoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(request);
  DCHECK_EQ(0U, request->url().find("http:"));
  DCHECK(callback);
  if (!callback) {
    DLOG(ERROR) << "ExtractFeatures called without a callback object";
    return;
  }
  pending_extractions_.insert(std::make_pair(request, callback));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BrowserFeatureExtractor::StartExtractFeatures,
          request, callback));
}

void BrowserFeatureExtractor::StartExtractFeatures(
    ClientPhishingRequest* request,
    DoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtractionData extraction = std::make_pair(request, callback);
  size_t removed = pending_extractions_.erase(extraction);
  DCHECK_EQ(1U, removed);
  HistoryService* history;
  if (!request || !request->IsInitialized() || !GetHistoryService(&history)) {
    callback->Run(false, request);
    return;
  }
  CancelableRequestProvider::Handle handle = history->QueryURL(
      GURL(request->url()),
      true /* wants_visits */,
      &request_consumer_,
      NewCallback(this,
                  &BrowserFeatureExtractor::QueryUrlHistoryDone));

  StorePendingQuery(handle, request, callback);
}

void BrowserFeatureExtractor::QueryUrlHistoryDone(
    CancelableRequestProvider::Handle handle,
    bool success,
    const history::URLRow* row,
    history::VisitVector* visits) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ClientPhishingRequest* request;
  DoneCallback* callback;
  if (!GetPendingQuery(handle, &request, &callback)) {
    DLOG(FATAL) << "No pending history query found";
    return;
  }
  DCHECK(request);
  DCHECK(callback);
  if (!success) {
    // URL is not found in the history.  In practice this should not
    // happen (unless there is a real error) because we just visited
    // that URL.
    callback->Run(false, request);
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
    if (!PageTransition::IsMainFrame(it->transition)) {
      continue;
    }
    if (it->visit_time < threshold) {
      ++num_visits_24h_ago;
    }
    PageTransition::Type transition = PageTransition::StripQualifier(
        it->transition);
    if (transition == PageTransition::TYPED) {
      ++num_visits_typed;
    } else if (transition == PageTransition::LINK) {
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
    callback->Run(false, request);
    return;
  }
  CancelableRequestProvider::Handle next_handle =
      history->GetVisibleVisitCountToHost(
          GURL(request->url()),
          &request_consumer_,
          NewCallback(this, &BrowserFeatureExtractor::QueryHttpHostVisitsDone));
  StorePendingQuery(next_handle, request, callback);
}

void BrowserFeatureExtractor::QueryHttpHostVisitsDone(
    CancelableRequestProvider::Handle handle,
    bool success,
    int num_visits,
    base::Time first_visit) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ClientPhishingRequest* request;
  DoneCallback* callback;
  if (!GetPendingQuery(handle, &request, &callback)) {
    DLOG(FATAL) << "No pending history query found";
    return;
  }
  DCHECK(request);
  DCHECK(callback);
  if (!success) {
    callback->Run(false, request);
    return;
  }
  SetHostVisitsFeatures(num_visits, first_visit, true, request);

  // Same lookup but for the HTTPS URL.
  HistoryService* history;
  if (!GetHistoryService(&history)) {
    callback->Run(false, request);
    return;
  }
  std::string https_url = request->url();
  CancelableRequestProvider::Handle next_handle =
      history->GetVisibleVisitCountToHost(
          GURL(https_url.replace(0, 5, "https:")),
          &request_consumer_,
          NewCallback(this,
                      &BrowserFeatureExtractor::QueryHttpsHostVisitsDone));
  StorePendingQuery(next_handle, request, callback);
}

void BrowserFeatureExtractor::QueryHttpsHostVisitsDone(
    CancelableRequestProvider::Handle handle,
    bool success,
    int num_visits,
    base::Time first_visit) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ClientPhishingRequest* request;
  DoneCallback* callback;
  if (!GetPendingQuery(handle, &request, &callback)) {
    DLOG(FATAL) << "No pending history query found";
    return;
  }
  DCHECK(request);
  DCHECK(callback);
  if (!success) {
    callback->Run(false, request);
    return;
  }
  SetHostVisitsFeatures(num_visits, first_visit, false, request);
  callback->Run(true, request);  // We're done with all the history lookups.
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
  AddFeature(
      is_http_query ?
      features::kFirstHttpHostVisitMoreThan24hAgo :
      features::kFirstHttpsHostVisitMoreThan24hAgo,
      (first_visit < (base::Time::Now() - base::TimeDelta::FromDays(1))) ?
      1.0 : 0.0,
      request);
}

void BrowserFeatureExtractor::StorePendingQuery(
    CancelableRequestProvider::Handle handle,
    ClientPhishingRequest* request,
    DoneCallback* callback) {
  DCHECK_EQ(0U, pending_queries_.count(handle));
  pending_queries_[handle] = std::make_pair(request, callback);
}

bool BrowserFeatureExtractor::GetPendingQuery(
    CancelableRequestProvider::Handle handle,
    ClientPhishingRequest** request,
    DoneCallback** callback) {
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
  if (tab_ && tab_->profile()) {
    *history = tab_->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (*history) {
      return true;
    }
  }
  VLOG(2) << "Unable to query history.  No history service available.";
  return false;
}
};  // namespace safe_browsing
