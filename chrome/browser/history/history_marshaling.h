// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data structures for communication between the history service on the main
// thread and the backend on the history thread.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__
#define CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__
#pragma once

#include "base/scoped_vector.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/page_usage_data.h"
#include "content/browser/cancelable_request.h"

namespace history {

// Querying -------------------------------------------------------------------

typedef CancelableRequest1<HistoryService::QueryURLCallback,
                           Tuple2<URLRow, VisitVector> >
    QueryURLRequest;

typedef CancelableRequest1<HistoryService::QueryHistoryCallback,
                           QueryResults>
    QueryHistoryRequest;

typedef CancelableRequest1<HistoryService::QueryRedirectsCallback,
                           history::RedirectList>
    QueryRedirectsRequest;

typedef CancelableRequest<HistoryService::GetVisitCountToHostCallback>
    GetVisitCountToHostRequest;

typedef CancelableRequest1<HistoryService::QueryTopURLsAndRedirectsCallback,
                           Tuple2<std::vector<GURL>,
                                  history::RedirectMap> >
    QueryTopURLsAndRedirectsRequest;

typedef CancelableRequest1<HistoryService::QueryMostVisitedURLsCallback,
                           history::MostVisitedURLList>
    QueryMostVisitedURLsRequest;

// Thumbnails -----------------------------------------------------------------

typedef CancelableRequest<HistoryService::ThumbnailDataCallback>
    GetPageThumbnailRequest;

// Favicons -------------------------------------------------------------------

typedef CancelableRequest<FaviconService::FaviconDataCallback>
    GetFavIconRequest;

// Downloads ------------------------------------------------------------------

typedef CancelableRequest1<HistoryService::DownloadQueryCallback,
                           std::vector<DownloadCreateInfo> >
    DownloadQueryRequest;

typedef CancelableRequest<HistoryService::DownloadCreateCallback>
    DownloadCreateRequest;

// Deletion --------------------------------------------------------------------

typedef CancelableRequest<HistoryService::ExpireHistoryCallback>
    ExpireHistoryRequest;

// Segment usage --------------------------------------------------------------

typedef CancelableRequest1<HistoryService::SegmentQueryCallback,
                           ScopedVector<PageUsageData> >
    QuerySegmentUsageRequest;

// Keyword search terms -------------------------------------------------------

typedef
    CancelableRequest1<HistoryService::GetMostRecentKeywordSearchTermsCallback,
                       std::vector<KeywordSearchTermVisit> >
    GetMostRecentKeywordSearchTermsRequest;

// Generic operations ---------------------------------------------------------

// The argument here is an input value, which is the task to run on the
// background thread. The callback is used to execute the portion of the task
// that executes on the main thread.
typedef CancelableRequest1<HistoryService::HistoryDBTaskCallback,
                           scoped_refptr<HistoryDBTask> >
    HistoryDBTaskRequest;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__
