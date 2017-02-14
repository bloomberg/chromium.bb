// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_data_fetcher.h"

#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using data_use_measurement::DataUseUserData;

namespace image_fetcher {

// An active image URL fetcher request. The struct contains the related requests
// state.
struct ImageDataFetcher::ImageDataFetcherRequest {
  ImageDataFetcherRequest(const ImageDataFetcherCallback& callback,
                          std::unique_ptr<net::URLFetcher> url_fetcher)
    : callback(callback),
      url_fetcher(std::move(url_fetcher)) {}

  ~ImageDataFetcherRequest() {}

  // The callback to run after the image data was fetched. The callback will
  // be run even if the image data could not be fetched successfully.
  ImageDataFetcherCallback callback;

  std::unique_ptr<net::URLFetcher> url_fetcher;
};

ImageDataFetcher::ImageDataFetcher(
    net::URLRequestContextGetter* url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter),
      data_use_service_name_(DataUseUserData::IMAGE_FETCHER_UNTAGGED),
      next_url_fetcher_id_(0) {}

ImageDataFetcher::~ImageDataFetcher() {}

void ImageDataFetcher::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  data_use_service_name_ = data_use_service_name;
}

void ImageDataFetcher::FetchImageData(
    const GURL& image_url,
    const ImageDataFetcherCallback& callback) {
  FetchImageData(
      image_url, callback, /*referrer=*/std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

void ImageDataFetcher::FetchImageData(
    const GURL& image_url,
    const ImageDataFetcherCallback& callback,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy) {
  std::unique_ptr<net::URLFetcher> url_fetcher = net::URLFetcher::Create(
      next_url_fetcher_id_++, image_url, net::URLFetcher::GET, this);

  DataUseUserData::AttachToFetcher(url_fetcher.get(), data_use_service_name_);

  std::unique_ptr<ImageDataFetcherRequest> request(
      new ImageDataFetcherRequest(callback, std::move(url_fetcher)));
  request->url_fetcher->SetRequestContext(url_request_context_getter_.get());
  request->url_fetcher->SetReferrer(referrer);
  request->url_fetcher->SetReferrerPolicy(referrer_policy);
  request->url_fetcher->Start();

  pending_requests_[request->url_fetcher.get()] = std::move(request);
}

void ImageDataFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  auto request_iter = pending_requests_.find(source);
  DCHECK(request_iter != pending_requests_.end());

  bool success = source->GetStatus().status() == net::URLRequestStatus::SUCCESS;

  RequestMetadata metadata;
  metadata.response_code = RESPONSE_CODE_INVALID;
  if (success && source->GetResponseHeaders()) {
    source->GetResponseHeaders()->GetMimeType(&metadata.mime_type);
    metadata.response_code = source->GetResponseHeaders()->response_code();
    success &= (metadata.response_code == net::HTTP_OK);
  }

  std::string image_data;
  if (success) {
    source->GetResponseAsString(&image_data);
  }
  request_iter->second->callback.Run(image_data, metadata);

  // Remove the finished request.
  pending_requests_.erase(request_iter);
}

}  // namespace image_fetcher
