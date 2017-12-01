// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_provision_fetcher.h"

#include "base/bind.h"

namespace media {

MojoProvisionFetcher::MojoProvisionFetcher(
    mojom::ProvisionFetcherPtr provision_fetcher_ptr)
    : provision_fetcher_ptr_(std::move(provision_fetcher_ptr)),
      weak_factory_(this) {
  DVLOG(1) << __func__;
}

MojoProvisionFetcher::~MojoProvisionFetcher() = default;

// ProvisionFetcher implementation:
void MojoProvisionFetcher::Retrieve(const std::string& default_url,
                                    const std::string& request_data,
                                    const ResponseCB& response_cb) {
  DVLOG(1) << __func__;
  provision_fetcher_ptr_->Retrieve(
      default_url, request_data,
      base::Bind(&MojoProvisionFetcher::OnResponse, weak_factory_.GetWeakPtr(),
                 response_cb));
}

void MojoProvisionFetcher::OnResponse(const ResponseCB& response_cb,
                                      bool success,
                                      const std::string& response) {
  response_cb.Run(success, response);
}

}  // namespace media
