// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_client.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/ssl_status.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeSecurityStateModelClient);

using security_state::SecurityStateModel;

namespace {

// Converts a content::SecurityStyle (an indicator of a request's
// overall security level computed by //content) into a
// SecurityStateModel::SecurityLevel (a finer-grained SecurityStateModel
// concept that can express all of SecurityStateModel's policies that
// //content doesn't necessarily know about).
SecurityStateModel::SecurityLevel GetSecurityLevelForSecurityStyle(
    content::SecurityStyle style) {
  switch (style) {
    case content::SECURITY_STYLE_UNKNOWN:
      NOTREACHED();
      return SecurityStateModel::NONE;
    case content::SECURITY_STYLE_UNAUTHENTICATED:
      return SecurityStateModel::NONE;
    case content::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return SecurityStateModel::SECURITY_ERROR;
    case content::SECURITY_STYLE_WARNING:
      // content currently doesn't use this style.
      NOTREACHED();
      return SecurityStateModel::SECURITY_WARNING;
    case content::SECURITY_STYLE_AUTHENTICATED:
      return SecurityStateModel::SECURE;
  }
  return SecurityStateModel::NONE;
}

// Note: This is a lossy operation. Not all of the policies that can be
// expressed by a SecurityLevel (a //chrome concept) can be expressed by
// a content::SecurityStyle.
content::SecurityStyle SecurityLevelToSecurityStyle(
    SecurityStateModel::SecurityLevel security_level) {
  switch (security_level) {
    case SecurityStateModel::NONE:
      return content::SECURITY_STYLE_UNAUTHENTICATED;
    case SecurityStateModel::SECURITY_WARNING:
    case SecurityStateModel::SECURITY_POLICY_WARNING:
      return content::SECURITY_STYLE_WARNING;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return content::SECURITY_STYLE_AUTHENTICATED;
    case SecurityStateModel::SECURITY_ERROR:
      return content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  }

  NOTREACHED();
  return content::SECURITY_STYLE_UNKNOWN;
}

}  // namespace

ChromeSecurityStateModelClient::ChromeSecurityStateModelClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      security_state_model_(new SecurityStateModel()) {
  security_state_model_->SetClient(this);
}

ChromeSecurityStateModelClient::~ChromeSecurityStateModelClient() {}

// static
content::SecurityStyle ChromeSecurityStateModelClient::GetSecurityStyle(
    const security_state::SecurityStateModel::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  const content::SecurityStyle security_style =
      SecurityLevelToSecurityStyle(security_info.security_level);

  security_style_explanations->ran_insecure_content_style =
      SecurityLevelToSecurityStyle(
          SecurityStateModel::kRanInsecureContentLevel);
  security_style_explanations->displayed_insecure_content_style =
      SecurityLevelToSecurityStyle(
          SecurityStateModel::kDisplayedInsecureContentLevel);

  // Check if the page is HTTP; if so, no explanations are needed. Note
  // that SECURITY_STYLE_UNAUTHENTICATED does not necessarily mean that
  // the page is loaded over HTTP, because the security style merely
  // represents how the embedder wishes to display the security state of
  // the page, and the embedder can choose to display HTTPS page as HTTP
  // if it wants to (for example, displaying deprecated crypto
  // algorithms with the same UI treatment as HTTP pages).
  security_style_explanations->scheme_is_cryptographic =
      security_info.scheme_is_cryptographic;
  if (!security_info.scheme_is_cryptographic) {
    return security_style;
  }

  if (security_info.sha1_deprecation_status ==
      SecurityStateModel::DEPRECATED_SHA1_MAJOR) {
    security_style_explanations->broken_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MAJOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MAJOR_SHA1_DESCRIPTION),
            security_info.cert_id));
  } else if (security_info.sha1_deprecation_status ==
             SecurityStateModel::DEPRECATED_SHA1_MINOR) {
    security_style_explanations->unauthenticated_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1_DESCRIPTION),
            security_info.cert_id));
  }

  security_style_explanations->ran_insecure_content =
      security_info.mixed_content_status ==
          SecurityStateModel::RAN_MIXED_CONTENT ||
      security_info.mixed_content_status ==
          SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;
  security_style_explanations->displayed_insecure_content =
      security_info.mixed_content_status ==
          SecurityStateModel::DISPLAYED_MIXED_CONTENT ||
      security_info.mixed_content_status ==
          SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;

  if (net::IsCertStatusError(security_info.cert_status)) {
    base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(
        net::MapCertStatusToNetError(security_info.cert_status)));

    content::SecurityStyleExplanation explanation(
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
        l10n_util::GetStringFUTF8(
            IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
        security_info.cert_id);

    if (net::IsCertStatusMinorError(security_info.cert_status))
      security_style_explanations->unauthenticated_explanations.push_back(
          explanation);
    else
      security_style_explanations->broken_explanations.push_back(explanation);
  } else {
    // If the certificate does not have errors and is not using
    // deprecated SHA1, then add an explanation that the certificate is
    // valid.
    if (security_info.sha1_deprecation_status ==
        SecurityStateModel::NO_DEPRECATED_SHA1) {
      security_style_explanations->secure_explanations.push_back(
          content::SecurityStyleExplanation(
              l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
              l10n_util::GetStringUTF8(
                  IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION),
              security_info.cert_id));
    }
  }

  if (security_info.is_secure_protocol_and_ciphersuite) {
    security_style_explanations->secure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_SECURE_PROTOCOL_AND_CIPHERSUITE),
            l10n_util::GetStringUTF8(
                IDS_SECURE_PROTOCOL_AND_CIPHERSUITE_DESCRIPTION)));
  }

  security_style_explanations->pkp_bypassed = security_info.pkp_bypassed;
  if (security_info.pkp_bypassed) {
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            "Public-Key Pinning Bypassed",
            "Public-key pinning was bypassed by a local root certificate."));
  }

  return security_style;
}

const SecurityStateModel::SecurityInfo&
ChromeSecurityStateModelClient::GetSecurityInfo() const {
  return security_state_model_->GetSecurityInfo();
}

bool ChromeSecurityStateModelClient::RetrieveCert(
    scoped_refptr<net::X509Certificate>* cert) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry)
    return false;
  return content::CertStore::GetInstance()->RetrieveCert(
      entry->GetSSL().cert_id, cert);
}

bool ChromeSecurityStateModelClient::UsedPolicyInstalledCertificate() {
#if defined(OS_CHROMEOS)
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (service && service->UsedPolicyCertificates())
    return true;
#endif
  return false;
}

bool ChromeSecurityStateModelClient::IsOriginSecure(const GURL& url) {
  return content::IsOriginSecure(url);
}

void ChromeSecurityStateModelClient::GetVisibleSecurityState(
    SecurityStateModel::VisibleSecurityState* state) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry ||
      entry->GetSSL().security_style == content::SECURITY_STYLE_UNKNOWN) {
    *state = SecurityStateModel::VisibleSecurityState();
    return;
  }

  state->initialized = true;
  state->url = entry->GetURL();
  const content::SSLStatus& ssl = entry->GetSSL();
  state->initial_security_level =
      GetSecurityLevelForSecurityStyle(ssl.security_style);
  state->cert_id = ssl.cert_id;
  state->cert_status = ssl.cert_status;
  state->connection_status = ssl.connection_status;
  state->security_bits = ssl.security_bits;
  state->pkp_bypassed = ssl.pkp_bypassed;
  state->sct_verify_statuses.clear();
  state->sct_verify_statuses.insert(state->sct_verify_statuses.end(),
                                    ssl.num_unknown_scts,
                                    net::ct::SCT_STATUS_LOG_UNKNOWN);
  state->sct_verify_statuses.insert(state->sct_verify_statuses.end(),
                                    ssl.num_invalid_scts,
                                    net::ct::SCT_STATUS_INVALID);
  state->sct_verify_statuses.insert(state->sct_verify_statuses.end(),
                                    ssl.num_valid_scts, net::ct::SCT_STATUS_OK);
  state->displayed_mixed_content =
      (ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT)
          ? true
          : false;
  state->ran_mixed_content =
      (ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT) ? true
                                                                      : false;
}
