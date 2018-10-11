// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_METRIC_RECORDER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_METRIC_RECORDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"

class GURL;

namespace content {

// SignedExchangePrefetchMetricRecorder records signed exchange prefetch and
// its usage metrics.
class SignedExchangePrefetchMetricRecorder final
    : public base::RefCountedThreadSafe<SignedExchangePrefetchMetricRecorder> {
 public:
  SignedExchangePrefetchMetricRecorder();

  void OnSignedExchangePrefetchFinished(const GURL& outer_url,
                                        net::Error error);

 private:
  friend class base::RefCountedThreadSafe<SignedExchangePrefetchMetricRecorder>;
  ~SignedExchangePrefetchMetricRecorder();

  DISALLOW_COPY_AND_ASSIGN(SignedExchangePrefetchMetricRecorder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_METRIC_RECORDER_H_
