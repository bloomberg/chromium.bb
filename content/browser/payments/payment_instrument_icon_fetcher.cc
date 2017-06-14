// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_instrument_icon_fetcher.h"

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace content {

namespace {

net::NetworkTrafficAnnotationTag g_traffic_annotation =
    net::DefineNetworkTrafficAnnotation("payment_instrument_icon_fetcher", R"(
        semantics {
          sender: "Web Payments"
          description:
            "Chromium downloads payment instrument icons when registering
             payment instruments."
          trigger:
            "When user navigates to a website to register web payment apps."
          data:
            "URL of the required icon to fetch. No user information is sent."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled in settings. Users can refuse
             to install web payment apps."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

PaymentInstrumentIconFetcher::PaymentInstrumentIconFetcher()
    : checking_image_object_index_(0) {}
PaymentInstrumentIconFetcher::~PaymentInstrumentIconFetcher() {}

void PaymentInstrumentIconFetcher::Start(
    const std::vector<payments::mojom::ImageObjectPtr>& image_objects,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentInstrumentIconFetcherCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& obj : image_objects) {
    image_objects_.emplace_back(payments::mojom::ImageObject::New(obj->src));
  }
  DCHECK_GT(image_objects_.size(), 0U);

  callback_ = std::move(callback);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&PaymentInstrumentIconFetcher::StartFromUIThread, this,
                     std::move(service_worker_context)));
}

void PaymentInstrumentIconFetcher::StartFromUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  url_request_context_getter_ =
      service_worker_context->storage_partition()->GetURLRequestContext();
  if (!url_request_context_getter_) {
    PostCallbackToIOThread(std::string());
    return;
  }

  FetchIcon();
}

void PaymentInstrumentIconFetcher::FetchIcon() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (checking_image_object_index_ >= image_objects_.size()) {
    PostCallbackToIOThread(std::string());
    return;
  }

  GURL* icon_src_url = &(image_objects_[checking_image_object_index_]->src);
  if (!icon_src_url->is_valid()) {
    checking_image_object_index_++;
    FetchIcon();
    return;
  }

  fetcher_ = net::URLFetcher::Create(*icon_src_url, net::URLFetcher::GET, this,
                                     g_traffic_annotation);
  fetcher_->SetRequestContext(url_request_context_getter_.get());
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetStopOnRedirect(true);
  fetcher_->Start();
}

void PaymentInstrumentIconFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(fetcher_.get(), source);
  std::unique_ptr<net::URLFetcher> free_fetcher = std::move(fetcher_);

  std::string data;
  if (!(source->GetStatus().is_success() &&
        source->GetResponseCode() == net::HTTP_OK &&
        source->GetResponseAsString(&data))) {
    checking_image_object_index_++;
    FetchIcon();
    return;
  }

  service_manager::mojom::ConnectorRequest connector_request;
  std::unique_ptr<service_manager::Connector> connector =
      service_manager::Connector::Create(&connector_request);
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorRequest(std::move(connector_request));

  std::vector<uint8_t> image_data(data.begin(), data.end());
  data_decoder::DecodeImage(
      connector.get(), image_data, data_decoder::mojom::ImageCodec::DEFAULT,
      false, data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::Bind(&PaymentInstrumentIconFetcher::DecodeImageCallback, this));
}

void PaymentInstrumentIconFetcher::DecodeImageCallback(const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (bitmap.drawsNothing()) {
    checking_image_object_index_++;
    FetchIcon();
    return;
  }

  gfx::Image decoded_image = gfx::Image::CreateFrom1xBitmap(bitmap);
  scoped_refptr<base::RefCountedMemory> raw_data = decoded_image.As1xPNGBytes();
  std::string base_64;
  base::Base64Encode(
      base::StringPiece(raw_data->front_as<char>(), raw_data->size()),
      &base_64);

  PostCallbackToIOThread(base_64);
}

void PaymentInstrumentIconFetcher::PostCallbackToIOThread(
    const std::string& decoded_data) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(std::move(callback_), decoded_data));
}

}  // namespace content
