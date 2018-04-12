// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class SignedExchangeCertFetcher;
class URLLoaderThrottle;

// An interface for creating SignedExchangeCertFetcher object.
class CONTENT_EXPORT SignedExchangeCertFetcherFactory {
 public:
  virtual ~SignedExchangeCertFetcherFactory();
  // Creates a SignedExchangeCertFetcher and starts fetching the certificate.
  // Can be called at most once.
  virtual std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& cert_url,
      bool force_fetch,
      SignedExchangeCertFetcher::CertificateCallback callback,
      const signed_exchange_utils::LogCallback& error_message_callback) = 0;

  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;
  static std::unique_ptr<SignedExchangeCertFetcherFactory> Create(
      url::Origin request_initiator,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_FACTORY_H_
