// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_navigation_throttle.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ssl/ssl_error_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/navigation_handle.h"
#include "net/cert/cert_status_flags.h"

SSLErrorNavigationThrottle::SSLErrorNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    SSLErrorNavigationThrottle::HandleSSLErrorCallback
        handle_ssl_error_callback)
    : content::NavigationThrottle(navigation_handle),
      ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      handle_ssl_error_callback_(std::move(handle_ssl_error_callback)),
      weak_ptr_factory_(this) {}

SSLErrorNavigationThrottle::~SSLErrorNavigationThrottle() {}

const char* SSLErrorNavigationThrottle::GetNameForLogging() {
  return "SSLErrorNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SSLErrorNavigationThrottle::WillFailRequest() {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kCommittedInterstitials));
  content::NavigationHandle* handle = navigation_handle();
  if (!handle->GetSSLInfo().has_value()) {
    return content::NavigationThrottle::PROCEED;
  }

  int cert_status = handle->GetSSLInfo().value().cert_status;
  if (!net::IsCertStatusError(cert_status) ||
      net::IsCertStatusMinorError(cert_status)) {
    return content::NavigationThrottle::PROCEED;
  }

  // We don't know whether SSLErrorHandler will call the ShowInterstitial()
  // callback synchronously, so we post a task that will run after this function
  // defers the navigation. This ensures that ShowInterstitial() can always
  // safely call CancelDeferredNavigation().
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(handle_ssl_error_callback_), handle->GetWebContents(),
          net::MapCertStatusToNetError(cert_status),
          handle->GetSSLInfo().value(), handle->GetURL(),
          handle->ShouldSSLErrorsBeFatal(), false,
          std::move(ssl_cert_reporter_),
          base::Callback<void(content::CertificateRequestResultType)>(),
          base::BindOnce(&SSLErrorNavigationThrottle::ShowInterstitial,
                         weak_ptr_factory_.GetWeakPtr())));

  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::DEFER);
}

void SSLErrorNavigationThrottle::ShowInterstitial(
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page) {
  content::NavigationHandle* handle = navigation_handle();
  int net_error =
      net::MapCertStatusToNetError(handle->GetSSLInfo().value().cert_status);

  // Get the error page content before giving up ownership of |blocking_page|.
  std::string error_page_content = blocking_page->GetHTMLContents();

  SSLErrorTabHelper::AssociateBlockingPage(handle->GetWebContents(),
                                           handle->GetNavigationId(),
                                           std::move(blocking_page));

  CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, static_cast<net::Error>(net_error),
      error_page_content));
}
