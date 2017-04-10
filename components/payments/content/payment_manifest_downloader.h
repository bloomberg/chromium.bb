// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_DOWNLOADER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_DOWNLOADER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace payments {

// Downloader of the payment method manifest and web-app manifest based on the
// payment method name that is a URL with HTTPS scheme, e.g.,
// https://bobpay.com.
//
// The downloader does not follow redirects. A download succeeds only if all
// HTTP response codes are 200.
class PaymentManifestDownloader : public net::URLFetcherDelegate {
 public:
  // The interface for receiving the result of downloading a manifest.
  class Delegate {
   public:
    // Called when a manifest has been successfully downloaded.
    virtual void OnManifestDownloadSuccess(const std::string& content) = 0;

    // Called when failed to download the manifest for any reason:
    //  - HTTP response code is not 200.
    //  - HTTP GET on the manifest URL returns empty content.
    //
    // In the case of a payment method manifest download, can also be called
    // when:
    //  - HTTP response headers are absent.
    //  - HTTP response headers do not contain Link headers.
    //  - Link header does not contain rel="payment-method-manifest".
    //  - Link header does not contain a valid URL.
    virtual void OnManifestDownloadFailure() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |delegate| should not be null and must outlive this object. |url| should be
  // a valid URL with HTTPS scheme.
  PaymentManifestDownloader(
      const scoped_refptr<net::URLRequestContextGetter>& context,
      const GURL& url,
      Delegate* delegate);

  ~PaymentManifestDownloader() override;

  // Download a payment method manifest via two consecutive HTTP requests:
  //
  // 1) HEAD request for the payment method name. The HTTP response header is
  //    parsed for Link header that points to the location of the payment method
  //    manifest file. Example of a relative location:
  //
  //      Link: <data/payment-manifest.json>; rel="payment-method-manifest"
  //
  //    (This is relative to the payment method URL.) Example of an absolute
  //    location:
  //
  //      Link: <https://bobpay.com/data/payment-manifest.json>;
  //      rel="payment-method-manifest"
  //
  //    The absolute location must use HTTPS scheme.
  //
  // 2) GET request for the payment method manifest file.
  void DownloadPaymentMethodManifest();

  // Download a web app manifest via a single HTTP request:
  //
  // 1) GET request for the payment method name.
  void DownloadWebAppManifest();

 private:
  void InitiateDownload(const GURL& url,
                        net::URLFetcher::RequestType request_type);

  // net::URLFetcherDelegate
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  scoped_refptr<net::URLRequestContextGetter> context_;
  const GURL url_;

  // Non-owned. Never null. Outlives this object.
  Delegate* delegate_;

  bool is_downloading_http_link_header_;
  std::unique_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestDownloader);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_DOWNLOADER_H_
