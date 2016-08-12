// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_policy.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace content {

namespace {

// Events for UMA. Do not reorder or change!
enum SSLGoodCertSeenEvent {
  NO_PREVIOUS_EXCEPTION = 0,
  HAD_PREVIOUS_EXCEPTION = 1,
  SSL_GOOD_CERT_SEEN_EVENT_MAX = 2
};

void OnAllowCertificate(SSLErrorHandler* handler,
                        const SSLPolicy* const policy,
                        CertificateRequestResultType decision) {
  DCHECK(handler->ssl_info().is_valid());
  switch (decision) {
    case CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE:
      // Note that we should not call SetMaxSecurityStyle here, because
      // the active NavigationEntry has just been deleted (in
      // HideInterstitialPage) and the new NavigationEntry will not be
      // set until DidNavigate.  This is ok, because the new
      // NavigationEntry will have its max security style set within
      // DidNavigate.
      //
      // While AllowCertForHost() executes synchronously on this thread,
      // ContinueRequest() gets posted to a different thread. Calling
      // AllowCertForHost() first ensures deterministic ordering.
      policy->backend()->AllowCertForHost(*handler->ssl_info().cert.get(),
                                          handler->request_url().host(),
                                          handler->cert_error());
      handler->ContinueRequest();
      return;
    case CERTIFICATE_REQUEST_RESULT_TYPE_DENY:
      handler->DenyRequest();
      return;
    case CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL:
      handler->CancelRequest();
      return;
  }
}

}  // namespace

SSLPolicy::SSLPolicy(SSLPolicyBackend* backend)
    : backend_(backend) {
  DCHECK(backend_);
}

void SSLPolicy::OnCertError(std::unique_ptr<SSLErrorHandler> handler) {
  bool expired_previous_decision = false;
  // First we check if we know the policy for this error.
  DCHECK(handler->ssl_info().is_valid());
  SSLHostStateDelegate::CertJudgment judgment =
      backend_->QueryPolicy(*handler->ssl_info().cert.get(),
                            handler->request_url().host(),
                            handler->cert_error(),
                            &expired_previous_decision);

  if (judgment == SSLHostStateDelegate::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  // For all other hosts, which must be DENIED, a blocking page is shown to the
  // user every time they come back to the page.
  int options_mask = 0;
  switch (handler->cert_error()) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
    case net::ERR_CERT_VALIDITY_TOO_LONG:
    case net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
      if (!handler->fatal())
        options_mask |= OVERRIDABLE;
      else
        options_mask |= STRICT_ENFORCEMENT;
      if (expired_previous_decision)
        options_mask |= EXPIRED_PREVIOUS_DECISION;
      OnCertErrorInternal(std::move(handler), options_mask);
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
    case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      if (handler->fatal())
        options_mask |= STRICT_ENFORCEMENT;
      if (expired_previous_decision)
        options_mask |= EXPIRED_PREVIOUS_DECISION;
      OnCertErrorInternal(std::move(handler), options_mask);
      break;
    default:
      NOTREACHED();
      handler->CancelRequest();
      break;
  }
}

void SSLPolicy::DidRunInsecureContent(NavigationEntryImpl* entry,
                                      const GURL& security_origin) {
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
      return;

  backend_->HostRanInsecureContent(security_origin.host(),
                                   site_instance->GetProcess()->GetID());
}

void SSLPolicy::DidRunContentWithCertErrors(NavigationEntryImpl* entry,
                                            const GURL& security_origin) {
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  backend_->HostRanContentWithCertErrors(security_origin.host(),
                                         site_instance->GetProcess()->GetID());
}

void SSLPolicy::OnRequestStarted(const GURL& url,
                                 int cert_id,
                                 net::CertStatus cert_status) {
  if (cert_id && url.SchemeIsCryptographic() &&
      !net::IsCertStatusError(cert_status)) {
    // If the scheme is https: or wss: *and* the security info for the
    // cert has been set (i.e. the cert id is not 0) and the cert did
    // not have any errors, revoke any previous decisions that
    // have occurred. If the cert info has not been set, do nothing since it
    // isn't known if the connection was actually a valid connection or if it
    // had a cert error.
    SSLGoodCertSeenEvent event = NO_PREVIOUS_EXCEPTION;
    if (backend_->HasAllowException(url.host())) {
      // If there's no certificate error, a good certificate has been seen, so
      // clear out any exceptions that were made by the user for bad
      // certificates. This intentionally does not apply to cached resources
      // (see https://crbug.com/634553 for an explanation).
      backend_->RevokeUserAllowExceptions(url.host());
      event = HAD_PREVIOUS_EXCEPTION;
    }
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.good_cert_seen", event,
                              SSL_GOOD_CERT_SEEN_EVENT_MAX);
  }
}

void SSLPolicy::UpdateEntry(NavigationEntryImpl* entry,
                            WebContents* web_contents) {
  DCHECK(entry);

  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);

  InitializeEntryIfNeeded(entry);

  if (entry->GetSSL().security_style == SECURITY_STYLE_UNAUTHENTICATED)
    return;

  if (!web_contents_impl->DisplayedInsecureContent())
    entry->GetSSL().content_status &= ~SSLStatus::DISPLAYED_INSECURE_CONTENT;

  if (web_contents_impl->DisplayedInsecureContent())
    entry->GetSSL().content_status |= SSLStatus::DISPLAYED_INSECURE_CONTENT;

  if (!web_contents_impl->DisplayedContentWithCertErrors())
    entry->GetSSL().content_status &=
        ~SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS;

  if (web_contents_impl->DisplayedContentWithCertErrors())
    entry->GetSSL().content_status |=
        SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS;

  SiteInstance* site_instance = entry->site_instance();
  // Note that |site_instance| can be NULL here because NavigationEntries don't
  // necessarily have site instances.  Without a process, the entry can't
  // possibly have insecure content.  See bug http://crbug.com/12423.
  if (site_instance &&
      backend_->DidHostRunInsecureContent(
          entry->GetURL().host(), site_instance->GetProcess()->GetID())) {
    entry->GetSSL().security_style =
        SECURITY_STYLE_AUTHENTICATION_BROKEN;
    entry->GetSSL().content_status |= SSLStatus::RAN_INSECURE_CONTENT;
  }

  if (site_instance &&
      backend_->DidHostRunContentWithCertErrors(
          entry->GetURL().host(), site_instance->GetProcess()->GetID())) {
    entry->GetSSL().security_style = SECURITY_STYLE_AUTHENTICATION_BROKEN;
    entry->GetSSL().content_status |= SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;
  }
}

// Static
SecurityStyle SSLPolicy::GetSecurityStyleForResource(
    const GURL& url,
    int cert_id,
    net::CertStatus cert_status) {
  // An HTTPS response may not have a certificate for some reason.  When that
  // happens, use the unauthenticated (HTTP) rather than the authentication
  // broken security style so that we can detect this error condition.
  if (!url.SchemeIsCryptographic() || !cert_id)
    return SECURITY_STYLE_UNAUTHENTICATED;

  // Minor errors don't lower the security style to
  // SECURITY_STYLE_AUTHENTICATION_BROKEN.
  if (net::IsCertStatusError(cert_status) &&
      !net::IsCertStatusMinorError(cert_status)) {
    return SECURITY_STYLE_AUTHENTICATION_BROKEN;
  }

  return SECURITY_STYLE_AUTHENTICATED;
}

////////////////////////////////////////////////////////////////////////////////
// Certificate Error Routines

void SSLPolicy::OnCertErrorInternal(std::unique_ptr<SSLErrorHandler> handler,
                                    int options_mask) {
  bool overridable = (options_mask & OVERRIDABLE) != 0;
  bool strict_enforcement = (options_mask & STRICT_ENFORCEMENT) != 0;
  bool expired_previous_decision =
      (options_mask & EXPIRED_PREVIOUS_DECISION) != 0;

  WebContents* web_contents = handler->web_contents();
  int cert_error = handler->cert_error();
  const net::SSLInfo& ssl_info = handler->ssl_info();
  const GURL& request_url = handler->request_url();
  ResourceType resource_type = handler->resource_type();
  GetContentClient()->browser()->AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url, resource_type,
      overridable, strict_enforcement, expired_previous_decision,
      base::Bind(&OnAllowCertificate, base::Owned(handler.release()), this));
}

void SSLPolicy::InitializeEntryIfNeeded(NavigationEntryImpl* entry) {
  if (entry->GetSSL().security_style != SECURITY_STYLE_UNKNOWN)
    return;

  entry->GetSSL().security_style = GetSecurityStyleForResource(
      entry->GetURL(), entry->GetSSL().cert_id, entry->GetSSL().cert_status);
}

}  // namespace content
