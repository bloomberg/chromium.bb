// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/file_downloader.h"

#include "base/callback.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace nacl {

FileDownloader::FileDownloader(scoped_ptr<blink::WebURLLoader> url_loader,
                               base::File file,
                               StatusCallback status_cb,
                               ProgressCallback progress_cb)
    : url_loader_(url_loader.Pass()),
      file_(file.Pass()),
      status_cb_(status_cb),
      progress_cb_(progress_cb),
      http_status_code_(-1),
      total_bytes_received_(0),
      total_bytes_to_be_received_(-1),
      status_(SUCCESS) {
  CHECK(!status_cb.is_null());
}

FileDownloader::~FileDownloader() {
}

void FileDownloader::Load(const blink::WebURLRequest& request) {
  url_loader_->loadAsynchronously(request, this);
}

void FileDownloader::didReceiveResponse(
    blink::WebURLLoader* loader,
    const blink::WebURLResponse& response) {
  http_status_code_ = response.httpStatusCode();
  if (http_status_code_ != 200)
    status_ = FAILED;

  // Set -1 if the content length is unknown. Set before issuing callback.
  total_bytes_to_be_received_ = response.expectedContentLength();
  if (!progress_cb_.is_null())
    progress_cb_.Run(total_bytes_received_, total_bytes_to_be_received_);
}

void FileDownloader::didReceiveData(
    blink::WebURLLoader* loader,
    const char* data,
    int data_length,
    int encoded_data_length) {
  if (status_ == SUCCESS) {
    if (file_.Write(total_bytes_received_, data, data_length) == -1) {
      status_ = FAILED;
      return;
    }
    total_bytes_received_ += data_length;
    if (!progress_cb_.is_null())
      progress_cb_.Run(total_bytes_received_, total_bytes_to_be_received_);
  }
}

void FileDownloader::didFinishLoading(
    blink::WebURLLoader* loader,
    double finish_time,
    int64_t total_encoded_data_length) {
  if (status_ == SUCCESS) {
    // Seek back to the beginning of the file that was just written so it's
    // easy for consumers to use.
    if (file_.Seek(base::File::FROM_BEGIN, 0) != 0)
      status_ = FAILED;
  }
  status_cb_.Run(status_, file_.Pass(), http_status_code_);
  delete this;
}

void FileDownloader::didFail(
    blink::WebURLLoader* loader,
    const blink::WebURLError& error) {
  status_ = FAILED;
  if (error.domain.equals(blink::WebString::fromUTF8(net::kErrorDomain))) {
    switch (error.reason) {
      case net::ERR_ACCESS_DENIED:
      case net::ERR_NETWORK_ACCESS_DENIED:
        status_ = ACCESS_DENIED;
        break;
    }
  } else {
    // It's a WebKit error.
    status_ = ACCESS_DENIED;
  }
}

}  // namespace nacl
