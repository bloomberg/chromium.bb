// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data structures for communication between the history service on the main
// thread and the backend on the history thread.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_ANDROID_H_
#define CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_ANDROID_H_
#pragma once

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/android/android_history_provider_service.h"

namespace history {

typedef CancelableRequest<AndroidHistoryProviderService::InsertCallback>
    InsertRequest;
typedef CancelableRequest<AndroidHistoryProviderService::QueryCallback>
    QueryRequest;
typedef CancelableRequest<AndroidHistoryProviderService::UpdateCallback>
    UpdateRequest;
typedef CancelableRequest<AndroidHistoryProviderService::DeleteCallback>
    DeleteRequest;
typedef CancelableRequest<AndroidHistoryProviderService::MoveStatementCallback>
    MoveStatementRequest;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_ANDROID_H_
