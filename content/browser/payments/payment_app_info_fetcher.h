// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/common/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class PaymentAppInfoFetcher
    : public base::RefCountedThreadSafe<PaymentAppInfoFetcher> {
 public:
  PaymentAppInfoFetcher();

  using PaymentAppInfoFetchCallback =
      base::OnceCallback<void(const std::string&, const std::string&)>;
  void Start(const GURL& context_url,
             scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
             PaymentAppInfoFetchCallback callback);

 private:
  friend class base::RefCountedThreadSafe<PaymentAppInfoFetcher>;
  ~PaymentAppInfoFetcher();

  void StartFromUIThread(
      const std::unique_ptr<std::vector<std::pair<int, int>>>& provider_hosts);

  // The WebContents::GetManifestCallback.
  void FetchPaymentAppManifestCallback(const GURL& url,
                                       const Manifest& manifest);

  // The ManifestIconDownloader::IconFetchCallback.
  void OnIconFetched(const SkBitmap& icon);
  void PostPaymentAppInfoFetchResultToIOThread();

  GURL context_url_;
  PaymentAppInfoFetchCallback callback_;

  int context_process_id_;
  int context_frame_id_;
  std::string fetched_payment_app_name_;
  std::string fetched_payment_app_icon_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppInfoFetcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
