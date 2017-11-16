// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class PaymentAppInfoFetcher
    : public base::RefCountedThreadSafe<PaymentAppInfoFetcher> {
 public:
  PaymentAppInfoFetcher();

  struct PaymentAppInfo {
    PaymentAppInfo();
    ~PaymentAppInfo();

    std::string name;
    std::string icon;
    bool prefer_related_applications = false;
    std::vector<StoredRelatedApplication> related_applications;
  };
  using PaymentAppInfoFetchCallback =
      base::OnceCallback<void(std::unique_ptr<PaymentAppInfo> app_info)>;
  void Start(const GURL& context_url,
             scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
             PaymentAppInfoFetchCallback callback);

 private:
  friend class base::RefCountedThreadSafe<PaymentAppInfoFetcher>;

  // Keeps track of the web contents.
  class WebContentsHelper : public WebContentsObserver {
   public:
    explicit WebContentsHelper(WebContents* web_contents);
    ~WebContentsHelper() override;
  };

  ~PaymentAppInfoFetcher();

  void StartFromUIThread(
      const std::unique_ptr<std::vector<std::pair<int, int>>>& provider_hosts);

  // The WebContents::GetManifestCallback.
  void FetchPaymentAppManifestCallback(const GURL& url,
                                       const Manifest& manifest);

  // The ManifestIconDownloader::IconFetchCallback.
  void OnIconFetched(const SkBitmap& icon);
  void PostPaymentAppInfoFetchResultToIOThread();

  // Prints the warning |message| in the DevTools console, if possible.
  // Otherwise logs the warning on command line.
  void WarnIfPossible(const std::string& message);

  GURL context_url_;
  PaymentAppInfoFetchCallback callback_;

  std::unique_ptr<WebContentsHelper> web_contents_helper_;
  std::unique_ptr<PaymentAppInfo> fetched_payment_app_info_;
  GURL manifest_url_;
  GURL icon_url_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppInfoFetcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
