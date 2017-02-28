// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/common_name_mismatch_handler.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/ssl_errors/error_classification.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_status.h"

CommonNameMismatchHandler::CommonNameMismatchHandler(
    const GURL& request_url,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : request_url_(request_url), request_context_(request_context) {}

CommonNameMismatchHandler::~CommonNameMismatchHandler() {}

// static
CommonNameMismatchHandler::TestingState
    CommonNameMismatchHandler::testing_state_ = NOT_TESTING;

void CommonNameMismatchHandler::CheckSuggestedUrl(
    const GURL& url,
    const CheckUrlCallback& callback) {
  // Should be used only in tests.
  if (testing_state_ == IGNORE_REQUESTS_FOR_TESTING)
    return;

  DCHECK(CalledOnValidThread());
  DCHECK(!IsCheckingSuggestedUrl());
  DCHECK(check_url_callback_.is_null());

  check_url_callback_ = callback;

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ssl_name_mismatch_lookup", R"(
        semantics {
          sender: "SSL Name Mismatch Handler"
          description:
            "If Chromium cannot make a secure connection to a site, this can "
            "be because the site is misconfigured. The site may be serving a "
            "security certificate intended for another site. If the SSL Common "
            "Name Mismatch Handling feature is enabled, Chromium will try to "
            "detect if one of the domains listed in the site's certificate is "
            "available by issuing requests to those domains. If the response "
            "indicates that an alternative site for which the certificate is "
            "valid is available, Chromium will automatically redirect the user "
            "to the alternative site."
          trigger: "Resource load."
          data: "An HTTP HEAD request to the alternative site."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: false
          setting:
            "Users can disable this feature by command line flag "
            "'--disable-feature=SSLCommonNameMismatchHandling'."
          policy_exception_justification:
            "Not implemented."
        })");

  url_fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::HEAD, this,
                                         traffic_annotation);
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetRequestContext(request_context_.get());

  // Can't safely use net::LOAD_DISABLE_CERT_REVOCATION_CHECKING here,
  // since then the connection may be reused without checking the cert.
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
}

// static
bool CommonNameMismatchHandler::GetSuggestedUrl(
    const GURL& request_url,
    const std::vector<std::string>& dns_names,
    GURL* suggested_url) {
  std::string www_mismatch_hostname;
  if (!ssl_errors::GetWWWSubDomainMatch(request_url, dns_names,
                                        &www_mismatch_hostname)) {
    return false;
  }
  // The full URL should be pinged, not just the new hostname. So, get the
  // |suggested_url| with the |request_url|'s hostname replaced with
  // new hostname. Keep resource path, query params the same.
  GURL::Replacements replacements;
  replacements.SetHostStr(www_mismatch_hostname);
  *suggested_url = request_url.ReplaceComponents(replacements);
  return true;
}

void CommonNameMismatchHandler::Cancel() {
  url_fetcher_.reset();
  check_url_callback_.Reset();
}

void CommonNameMismatchHandler::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsCheckingSuggestedUrl());
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(!check_url_callback_.is_null());
  DCHECK(!url_fetcher_.get()->GetStatus().is_io_pending());

  SuggestedUrlCheckResult result = SUGGESTED_URL_NOT_AVAILABLE;
  // Save a copy of |suggested_url| so it can be used after |url_fetcher_|
  // is destroyed.
  const GURL suggested_url = url_fetcher_->GetOriginalURL();
  const GURL& landing_url = url_fetcher_->GetURL();

  // Make sure the |landing_url| is a HTTPS page and returns a proper response
  // code.
  if (url_fetcher_.get()->GetResponseCode() == 200 &&
      landing_url.SchemeIsCryptographic() &&
      landing_url.host() != request_url_.host()) {
    result = SUGGESTED_URL_AVAILABLE;
  }
  url_fetcher_.reset();
  base::ResetAndReturn(&check_url_callback_).Run(result, suggested_url);
}

bool CommonNameMismatchHandler::IsCheckingSuggestedUrl() const {
  return !!url_fetcher_;
}
