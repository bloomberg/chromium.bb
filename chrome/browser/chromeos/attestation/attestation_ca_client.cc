// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace {

const char kCertificateRequestURL[] = "https://chromeos-ca.gstatic.com/sign";
const char kEnrollRequestURL[] = "https://chromeos-ca.gstatic.com/enroll";
const char kMimeContentType[] = "application/octet-stream";

}  // namespace

namespace chromeos {
namespace attestation {

AttestationCAClient::AttestationCAClient() {}

AttestationCAClient::~AttestationCAClient() {}

void AttestationCAClient::SendEnrollRequest(const std::string& request,
                                            const DataCallback& on_response) {
  FetchURL(kEnrollRequestURL, request, on_response);
}

void AttestationCAClient::SendCertificateRequest(
    const std::string& request,
    const DataCallback& on_response) {
  FetchURL(kCertificateRequestURL, request, on_response);
}

void AttestationCAClient::OnURLFetchComplete(const net::URLFetcher* source) {
  FetcherCallbackMap::iterator iter = pending_requests_.find(source);
  if (iter == pending_requests_.end()) {
    LOG(WARNING) << "Callback from unknown source.";
    return;
  }

  DataCallback callback = iter->second;
  pending_requests_.erase(iter);
  scoped_ptr<const net::URLFetcher> scoped_source(source);

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    LOG(ERROR) << "Attestation CA request failed, status: "
               << source->GetStatus().status() << ", error: "
               << source->GetStatus().error();
    callback.Run(false, "");
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    LOG(ERROR) << "Attestation CA sent an error response: "
               << source->GetResponseCode();
    callback.Run(false, "");
    return;
  }

  std::string response;
  bool result = source->GetResponseAsString(&response);
  DCHECK(result) << "Invalid fetcher setting.";

  // Run the callback last because it may delete |this|.
  callback.Run(true, response);
}

void AttestationCAClient::FetchURL(const std::string& url,
                                   const std::string& request,
                                   const DataCallback& on_response) {
  // The first argument allows the use of TestURLFetcherFactory in tests.
  net::URLFetcher* fetcher = net::URLFetcher::Create(0,
                                                     GURL(url),
                                                     net::URLFetcher::POST,
                                                     this);
  fetcher->SetRequestContext(g_browser_process->system_request_context());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DISABLE_CACHE);
  fetcher->SetUploadData(kMimeContentType, request);
  pending_requests_[fetcher] = on_response;
  fetcher->Start();
}

}  // namespace attestation
}  // namespace chromeos
