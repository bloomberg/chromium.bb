// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data structures for communication between the history service on the main
// thread and the backend on the history thread.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H_
#define CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H_

#include "base/memory/scoped_vector.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/page_usage_data.h"

#if defined(OS_ANDROID)
#include "chrome/browser/history/history_marshaling_android.h"
#endif

namespace history {

// Querying -------------------------------------------------------------------

typedef CancelableRequest1<HistoryService::QueryMostVisitedURLsCallback,
                           history::MostVisitedURLList>
    QueryMostVisitedURLsRequest;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H_
