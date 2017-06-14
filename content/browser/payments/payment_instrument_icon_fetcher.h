// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_app.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class PaymentInstrumentIconFetcher
    : public base::RefCountedThreadSafe<PaymentInstrumentIconFetcher>,
      private net::URLFetcherDelegate {
 public:
  using PaymentInstrumentIconFetcherCallback =
      base::OnceCallback<void(const std::string&)>;

  PaymentInstrumentIconFetcher();

  // Starts fetching and decoding payment instrument icon from online. The
  // result will be send back through |callback|.
  // TODO(gogerald): Right now, we return the first fetchable and decodable icon
  // with smallest available size. We may add more logic to choose appropriate
  // icon according to icon size and our usage.
  void Start(const std::vector<payments::mojom::ImageObjectPtr>& image_objects,
             scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
             PaymentInstrumentIconFetcherCallback callback);

 private:
  friend class base::RefCountedThreadSafe<PaymentInstrumentIconFetcher>;
  ~PaymentInstrumentIconFetcher() override;

  void StartFromUIThread(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  void PostCallbackToIOThread(const std::string& decoded_data);

  // data_decoder::mojom::ImageDecoder::DecodeImageCallback.
  void DecodeImageCallback(const SkBitmap& bitmap);

  // Override net::URLFetcherDelegate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void FetchIcon();

  // Declared set of image objects of the payment instrument.
  std::vector<payments::mojom::ImageObjectPtr> image_objects_;

  // The index of the currently checking image object in image_objects_.
  size_t checking_image_object_index_;

  // The callback to notify after complete.
  PaymentInstrumentIconFetcherCallback callback_;

  // The url request context getter for fetching icon from online.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The url fetcher to fetch raw icon from online.
  std::unique_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PaymentInstrumentIconFetcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_