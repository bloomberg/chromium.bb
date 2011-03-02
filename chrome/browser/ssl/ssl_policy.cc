// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_policy.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ssl/ssl_request_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_info.h"
#include "webkit/glue/resource_type.h"

namespace {

static const char kDot = '.';

static bool IsIntranetHost(const std::string& host) {
  const size_t dot = host.find(kDot);
  return dot == std::string::npos || dot == host.length() - 1;
}

}  // namespace

SSLPolicy::SSLPolicy(SSLPolicyBackend* backend)
    : backend_(backend) {
  DCHECK(backend_);
}

void SSLPolicy::OnCertError(SSLCertErrorHandler* handler) {
  // First we check if we know the policy for this error.
  net::CertPolicy::Judgment judgment =
      backend_->QueryPolicy(handler->ssl_info().cert,
                            handler->request_url().host());

  if (judgment == net::CertPolicy::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  // The judgment is either DENIED or UNKNOWN.
  // For now we handle the DENIED as the UNKNOWN, which means a blocking
  // page is shown to the user every time he comes back to the page.

  switch (handler->cert_error()) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
      OnCertErrorInternal(handler, SSLBlockingPage::ERROR_OVERRIDABLE);
      break;
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
      // Ignore this error.
      handler->ContinueRequest();
      break;
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      // We ignore this error but will show a warning status in the location
      // bar.
      handler->ContinueRequest();
      break;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_CERT_NOT_IN_DNS:
      OnCertErrorInternal(handler, SSLBlockingPage::ERROR_FATAL);
      break;
    default:
      NOTREACHED();
      handler->CancelRequest();
      break;
  }
}

void SSLPolicy::DidRunInsecureContent(NavigationEntry* entry,
                                      const std::string& security_origin) {
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
      return;

  backend_->HostRanInsecureContent(GURL(security_origin).host(),
                                   site_instance->GetProcess()->id());
}

void SSLPolicy::OnRequestStarted(SSLRequestInfo* info) {
  // TODO(abarth): This mechanism is wrong.  What we should be doing is sending
  // this information back through WebKit and out some FrameLoaderClient
  // methods.

  if (net::IsCertStatusError(info->ssl_cert_status()))
    backend_->HostRanInsecureContent(info->url().host(), info->child_id());
}

void SSLPolicy::UpdateEntry(NavigationEntry* entry, TabContents* tab_contents) {
  DCHECK(entry);

  InitializeEntryIfNeeded(entry);

  if (!entry->url().SchemeIsSecure())
    return;

  // An HTTPS response may not have a certificate for some reason.  When that
  // happens, use the unauthenticated (HTTP) rather than the authentication
  // broken security style so that we can detect this error condition.
  if (!entry->ssl().cert_id()) {
    entry->ssl().set_security_style(SECURITY_STYLE_UNAUTHENTICATED);
    return;
  }

  if (!(entry->ssl().cert_status() & net::CERT_STATUS_COMMON_NAME_INVALID)) {
    // CAs issue certificates for intranet hosts to everyone.  Therefore, we
    // mark intranet hosts as being non-unique.
    if (IsIntranetHost(entry->url().host())) {
      entry->ssl().set_cert_status(entry->ssl().cert_status() |
                                   net::CERT_STATUS_NON_UNIQUE_NAME);
    }
  }

  // If CERT_STATUS_UNABLE_TO_CHECK_REVOCATION is the only certificate error,
  // don't lower the security style to SECURITY_STYLE_AUTHENTICATION_BROKEN.
  int cert_errors = entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS;
  if (cert_errors) {
    if (cert_errors != net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION)
      entry->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
    return;
  }

  SiteInstance* site_instance = entry->site_instance();
  // Note that |site_instance| can be NULL here because NavigationEntries don't
  // necessarily have site instances.  Without a process, the entry can't
  // possibly have insecure content.  See bug http://crbug.com/12423.
  if (site_instance &&
      backend_->DidHostRunInsecureContent(entry->url().host(),
                                          site_instance->GetProcess()->id())) {
    entry->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
    entry->ssl().set_ran_insecure_content();
    return;
  }

  if (tab_contents->displayed_insecure_content())
    entry->ssl().set_displayed_insecure_content();
}

////////////////////////////////////////////////////////////////////////////////
// SSLBlockingPage::Delegate methods

SSLErrorInfo SSLPolicy::GetSSLErrorInfo(SSLCertErrorHandler* handler) {
  return SSLErrorInfo::CreateError(
      SSLErrorInfo::NetErrorToErrorType(handler->cert_error()),
      handler->ssl_info().cert, handler->request_url());
}

void SSLPolicy::OnDenyCertificate(SSLCertErrorHandler* handler) {
  // Default behavior for rejecting a certificate.
  //
  // While DenyCertForHost() executes synchronously on this thread,
  // CancelRequest() gets posted to a different thread. Calling
  // DenyCertForHost() first ensures deterministic ordering.
  backend_->DenyCertForHost(handler->ssl_info().cert,
                            handler->request_url().host());
  handler->CancelRequest();
}

void SSLPolicy::OnAllowCertificate(SSLCertErrorHandler* handler) {
  // Default behavior for accepting a certificate.
  // Note that we should not call SetMaxSecurityStyle here, because the active
  // NavigationEntry has just been deleted (in HideInterstitialPage) and the
  // new NavigationEntry will not be set until DidNavigate.  This is ok,
  // because the new NavigationEntry will have its max security style set
  // within DidNavigate.
  //
  // While AllowCertForHost() executes synchronously on this thread,
  // ContinueRequest() gets posted to a different thread. Calling
  // AllowCertForHost() first ensures deterministic ordering.
  backend_->AllowCertForHost(handler->ssl_info().cert,
                             handler->request_url().host());
  handler->ContinueRequest();
}

////////////////////////////////////////////////////////////////////////////////
// Certificate Error Routines

void SSLPolicy::OnCertErrorInternal(SSLCertErrorHandler* handler,
                                    SSLBlockingPage::ErrorLevel error_level) {
  if (handler->resource_type() != ResourceType::MAIN_FRAME) {
    // A sub-resource has a certificate error.  The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    handler->DenyRequest();
    return;
  }
  SSLBlockingPage* blocking_page = new SSLBlockingPage(handler, this,
                                                       error_level);
  blocking_page->Show();
}

void SSLPolicy::InitializeEntryIfNeeded(NavigationEntry* entry) {
  if (entry->ssl().security_style() != SECURITY_STYLE_UNKNOWN)
    return;

  entry->ssl().set_security_style(entry->url().SchemeIsSecure() ?
      SECURITY_STYLE_AUTHENTICATED : SECURITY_STYLE_UNAUTHENTICATED);
}

void SSLPolicy::OriginRanInsecureContent(const std::string& origin, int pid) {
  GURL parsed_origin(origin);
  if (parsed_origin.SchemeIsSecure())
    backend_->HostRanInsecureContent(parsed_origin.host(), pid);
}
