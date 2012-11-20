// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BrowserFeatureExtractor computes various browser features for client-side
// phishing detection.  For now it does a bunch of lookups in the history
// service to see whether a particular URL has been visited before by the
// user.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURE_EXTRACTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURE_EXTRACTOR_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "googleurl/src/gurl.h"

class HistoryService;

namespace content {
class WebContents;
}

namespace safe_browsing {
class ClientPhishingRequest;
class ClientSideDetectionService;

struct BrowseInfo {
  // List of IPv4 and IPv6 addresses from which content was requested
  // while browsing to the |url|.
  std::set<std::string> ips;

  // If a SafeBrowsing interstitial was shown for the current URL
  // this will contain the UnsafeResource struct for that URL.
  scoped_ptr<SafeBrowsingUIManager::UnsafeResource> unsafe_resource;

  // List of redirects that lead to the first page on the current host and
  // the current url respectively. These may be the same if the current url
  // is the first page on its host.
  std::vector<GURL> host_redirects;
  std::vector<GURL> url_redirects;

  // The HTTP status code from this navigation.
  int http_status_code;

  BrowseInfo();
  ~BrowseInfo();
};

// All methods of this class must be called on the UI thread (including
// the constructor).
class BrowserFeatureExtractor {
 public:
  // Called when feature extraction is done.  The first argument will be
  // true iff feature extraction succeeded.  The second argument is the
  // phishing request which was modified by the feature extractor.  The
  // DoneCallback takes ownership of the request object.
  typedef base::Callback<void(bool, ClientPhishingRequest*)> DoneCallback;

  // The caller keeps ownership of the tab and service objects and is
  // responsible for ensuring that they stay valid for the entire
  // lifetime of this object.
  BrowserFeatureExtractor(content::WebContents* tab,
                          ClientSideDetectionService* service);

  // The destructor will cancel any pending requests.
  virtual ~BrowserFeatureExtractor();

  // Begins extraction of the browser features.  We take ownership
  // of the request object until |callback| is called (see DoneCallback above)
  // and will write the extracted features to the feature map.  Once the
  // feature extraction is complete, |callback| is run on the UI thread.  We
  // take ownership of the |callback| object.  |info| may not be valid after
  // ExtractFeatures returns.  This method must run on the UI thread.
  virtual void ExtractFeatures(const BrowseInfo* info,
                               ClientPhishingRequest* request,
                               const DoneCallback& callback);

 private:
  friend class base::DeleteHelper<BrowserFeatureExtractor>;
  typedef std::pair<ClientPhishingRequest*, DoneCallback> ExtractionData;
  typedef std::map<CancelableRequestProvider::Handle,
                   ExtractionData> PendingQueriesMap;

  // Synchronous browser feature extraction.
  void ExtractBrowseInfoFeatures(const BrowseInfo& info,
                                 ClientPhishingRequest* request);

  // Actually starts feature extraction (does the real work).
  void StartExtractFeatures(ClientPhishingRequest* request,
                            const DoneCallback& callback);

  // HistoryService callback which is called when we're done querying URL visits
  // in the history.
  void QueryUrlHistoryDone(CancelableRequestProvider::Handle handle,
                           bool success,
                           const history::URLRow* row,
                           history::VisitVector* visits);

  // HistoryService callback which is called when we're done querying HTTP host
  // visits in the history.
  void QueryHttpHostVisitsDone(CancelableRequestProvider::Handle handle,
                               bool success,
                               int num_visits,
                               base::Time first_visit);

  // HistoryService callback which is called when we're done querying HTTPS host
  // visits in the history.
  void QueryHttpsHostVisitsDone(CancelableRequestProvider::Handle handle,
                                bool success,
                                int num_visits,
                                base::Time first_visit);

  // Helper function which sets the host history features given the
  // number of host visits and the time of the fist host visit.  Set
  // |is_http_query| to true if the URL scheme is HTTP and to false if
  // the scheme is HTTPS.
  void SetHostVisitsFeatures(int num_visits,
                             base::Time first_visit,
                             bool is_http_query,
                             ClientPhishingRequest* request);

  // Helper function which stores the request and callback while the history
  // query is being processed.
  void StorePendingQuery(CancelableRequestProvider::Handle handle,
                         ClientPhishingRequest* request,
                         const DoneCallback& callback);

  // Helper function which is the counterpart of StorePendingQuery.  If there
  // is a pending query for the given handle it will return false and set both
  // the request and cb pointers.  Otherwise, it will return false.
  bool GetPendingQuery(CancelableRequestProvider::Handle handle,
                       ClientPhishingRequest** request,
                       DoneCallback* callback);

  // Helper function which gets the history server if possible.  If the pointer
  // is set it will return true and false otherwise.
  bool GetHistoryService(HistoryService** history);

  content::WebContents* tab_;
  ClientSideDetectionService* service_;
  CancelableRequestConsumer request_consumer_;
  base::WeakPtrFactory<BrowserFeatureExtractor> weak_factory_;

  // Set of pending extractions (i.e. extractions for which ExtractFeatures was
  // called but not StartExtractFeatures).
  std::map<ClientPhishingRequest*, DoneCallback> pending_extractions_;

  // Set of pending queries (i.e., where history->Query...() was called but
  // the history callback hasn't been invoked yet).
  PendingQueriesMap pending_queries_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFeatureExtractor);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURE_EXTRACTOR_H_
