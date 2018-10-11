// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"

#include "content/public/browser/browser_thread.h"

namespace content {

SignedExchangePrefetchMetricRecorder::SignedExchangePrefetchMetricRecorder() =
    default;
SignedExchangePrefetchMetricRecorder::~SignedExchangePrefetchMetricRecorder() =
    default;

void SignedExchangePrefetchMetricRecorder::OnSignedExchangePrefetchFinished(
    const GURL& outer_url,
    net::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(crbug.com/890180): Actually Report UMA.
}

}  // namespace content
