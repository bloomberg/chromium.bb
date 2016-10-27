// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_client.h"

#include <openssl/ssl.h>

#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeSecurityStateModelClient);

using safe_browsing::SafeBrowsingUIManager;
using security_state::SecurityStateModel;

namespace {

// Note: This is a lossy operation. Not all of the policies that can be
// expressed by a SecurityLevel (a //chrome concept) can be expressed by
// a blink::WebSecurityStyle.
blink::WebSecurityStyle SecurityLevelToSecurityStyle(
    SecurityStateModel::SecurityLevel security_level) {
  switch (security_level) {
    case SecurityStateModel::NONE:
    case SecurityStateModel::HTTP_SHOW_WARNING:
      return blink::WebSecurityStyleUnauthenticated;
    case SecurityStateModel::SECURITY_WARNING:
    case SecurityStateModel::SECURE_WITH_POLICY_INSTALLED_CERT:
      return blink::WebSecurityStyleWarning;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return blink::WebSecurityStyleAuthenticated;
    case SecurityStateModel::DANGEROUS:
      return blink::WebSecurityStyleAuthenticationBroken;
  }

  NOTREACHED();
  return blink::WebSecurityStyleUnknown;
}

void AddConnectionExplanation(
    const security_state::SecurityStateModel::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {

  // Avoid showing TLS details when we couldn't even establish a TLS connection
  // (e.g. for net errors) or if there was no real connection (some tests). We
  // check the |connection_status| to see if there was a connection.
  if (security_info.connection_status == 0) {
    return;
  }

  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);
  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(security_info.connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  base::string16 protocol_name = base::ASCIIToUTF16(protocol);
  const base::string16 cipher_name =
      (mac == NULL) ? base::ASCIIToUTF16(cipher)
                    : l10n_util::GetStringFUTF16(IDS_CIPHER_WITH_MAC,
                                                 base::ASCIIToUTF16(cipher),
                                                 base::ASCIIToUTF16(mac));

  // Include the key exchange group (previously known as curve) if specified.
  base::string16 key_exchange_name;
  if (is_tls13) {
    key_exchange_name = base::ASCIIToUTF16(
        SSL_get_curve_name(security_info.key_exchange_group));
  } else if (security_info.key_exchange_group != 0) {
    key_exchange_name = l10n_util::GetStringFUTF16(
        IDS_SSL_KEY_EXCHANGE_WITH_GROUP, base::ASCIIToUTF16(key_exchange),
        base::ASCIIToUTF16(
            SSL_get_curve_name(security_info.key_exchange_group)));
  } else {
    key_exchange_name = base::ASCIIToUTF16(key_exchange);
  }

  if (security_info.obsolete_ssl_status == net::OBSOLETE_SSL_NONE) {
    security_style_explanations->secure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_STRONG_SSL_SUMMARY),
            l10n_util::GetStringFUTF8(IDS_STRONG_SSL_DESCRIPTION, protocol_name,
                                      key_exchange_name, cipher_name)));
    return;
  }

  std::vector<base::string16> description_replacements;
  int status = security_info.obsolete_ssl_status;
  int str_id;

  str_id = (status & net::OBSOLETE_SSL_MASK_PROTOCOL)
               ? IDS_SSL_AN_OBSOLETE_PROTOCOL
               : IDS_SSL_A_STRONG_PROTOCOL;
  description_replacements.push_back(l10n_util::GetStringUTF16(str_id));
  description_replacements.push_back(protocol_name);

  str_id = (status & net::OBSOLETE_SSL_MASK_KEY_EXCHANGE)
               ? IDS_SSL_AN_OBSOLETE_KEY_EXCHANGE
               : IDS_SSL_A_STRONG_KEY_EXCHANGE;
  description_replacements.push_back(l10n_util::GetStringUTF16(str_id));
  description_replacements.push_back(key_exchange_name);

  str_id = (status & net::OBSOLETE_SSL_MASK_CIPHER) ? IDS_SSL_AN_OBSOLETE_CIPHER
                                                    : IDS_SSL_A_STRONG_CIPHER;
  description_replacements.push_back(l10n_util::GetStringUTF16(str_id));
  description_replacements.push_back(cipher_name);

  security_style_explanations->info_explanations.push_back(
      content::SecurityStyleExplanation(
          l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY),
          base::UTF16ToUTF8(
              l10n_util::GetStringFUTF16(IDS_OBSOLETE_SSL_DESCRIPTION,
                                         description_replacements, nullptr))));
}

// Check to see whether the security state should be downgraded to reflect
// a Safe Browsing verdict.
void CheckSafeBrowsingStatus(content::NavigationEntry* entry,
                             content::WebContents* web_contents,
                             SecurityStateModel::VisibleSecurityState* state) {
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service)
    return;
  scoped_refptr<SafeBrowsingUIManager> sb_ui_manager = sb_service->ui_manager();
  if (sb_ui_manager->IsUrlWhitelistedOrPendingForWebContents(
          entry->GetURL(), false, entry, web_contents, false)) {
    state->fails_malware_check = true;
  }
}

}  // namespace

ChromeSecurityStateModelClient::ChromeSecurityStateModelClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      security_state_model_(new SecurityStateModel()),
      logged_http_warning_on_current_navigation_(false) {
  security_state_model_->SetClient(this);
}

ChromeSecurityStateModelClient::~ChromeSecurityStateModelClient() {}

// static
blink::WebSecurityStyle ChromeSecurityStateModelClient::GetSecurityStyle(
    const security_state::SecurityStateModel::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  const blink::WebSecurityStyle security_style =
      SecurityLevelToSecurityStyle(security_info.security_level);

  if (security_info.security_level ==
      security_state::SecurityStateModel::HTTP_SHOW_WARNING) {
    // If the HTTP_SHOW_WARNING field trial is in use, display an
    // unauthenticated explanation explaining why the omnibox warning is
    // present.
    security_style_explanations->unauthenticated_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT),
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT_DESCRIPTION)));
  } else if (security_info.security_level ==
                 security_state::SecurityStateModel::NONE &&
             security_info.displayed_private_user_data_input_on_http) {
    // If the HTTP_SHOW_WARNING field trial isn't in use yet, display an
    // informational note that the omnibox will contain a warning for
    // this site in a future version of Chrome.
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT),
            l10n_util::GetStringUTF8(
                IDS_PRIVATE_USER_DATA_INPUT_FUTURE_DESCRIPTION)));
  }

  security_style_explanations->ran_insecure_content_style =
      SecurityLevelToSecurityStyle(
          SecurityStateModel::kRanInsecureContentLevel);
  security_style_explanations->displayed_insecure_content_style =
      SecurityLevelToSecurityStyle(
          SecurityStateModel::kDisplayedInsecureContentLevel);

  // Check if the page is HTTP; if so, no more explanations are needed. Note
  // that SecurityStyleUnauthenticated does not necessarily mean that
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
            !!security_info.certificate));
  } else if (security_info.sha1_deprecation_status ==
             SecurityStateModel::DEPRECATED_SHA1_MINOR) {
    security_style_explanations->unauthenticated_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1_DESCRIPTION),
            !!security_info.certificate));
  }

  // Record the presence of mixed content (HTTP subresources on an HTTPS
  // page).
  security_style_explanations->ran_mixed_content =
      security_info.mixed_content_status ==
          SecurityStateModel::CONTENT_STATUS_RAN ||
      security_info.mixed_content_status ==
          SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  security_style_explanations->displayed_mixed_content =
      security_info.mixed_content_status ==
          SecurityStateModel::CONTENT_STATUS_DISPLAYED ||
      security_info.mixed_content_status ==
          SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;

  bool is_cert_status_error = net::IsCertStatusError(security_info.cert_status);
  bool is_cert_status_minor_error =
      net::IsCertStatusMinorError(security_info.cert_status);

  // If the main resource was loaded no certificate errors or only minor
  // certificate errors, then record the presence of subresources with
  // certificate errors. Subresource certificate errors aren't recorded
  // when the main resource was loaded with major certificate errors
  // because, in the common case, these subresource certificate errors
  // would be duplicative with the main resource's error.
  if (!is_cert_status_error || is_cert_status_minor_error) {
    security_style_explanations->ran_content_with_cert_errors =
        security_info.content_with_cert_errors_status ==
            SecurityStateModel::CONTENT_STATUS_RAN ||
        security_info.content_with_cert_errors_status ==
            SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
    security_style_explanations->displayed_content_with_cert_errors =
        security_info.content_with_cert_errors_status ==
            SecurityStateModel::CONTENT_STATUS_DISPLAYED ||
        security_info.content_with_cert_errors_status ==
            SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  }

  if (is_cert_status_error) {
    base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(
        net::MapCertStatusToNetError(security_info.cert_status)));

    content::SecurityStyleExplanation explanation(
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
        l10n_util::GetStringFUTF8(
            IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
        !!security_info.certificate);

    if (is_cert_status_minor_error) {
      security_style_explanations->unauthenticated_explanations.push_back(
          explanation);
    } else {
      security_style_explanations->broken_explanations.push_back(explanation);
    }
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
              !!security_info.certificate));
    }
  }

  AddConnectionExplanation(security_info, security_style_explanations);

  security_style_explanations->pkp_bypassed = security_info.pkp_bypassed;
  if (security_info.pkp_bypassed) {
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            "Public-Key Pinning Bypassed",
            "Public-key pinning was bypassed by a local root certificate."));
  }

  return security_style;
}

void ChromeSecurityStateModelClient::GetSecurityInfo(
    SecurityStateModel::SecurityInfo* result) const {
  security_state_model_->GetSecurityInfo(result);
}

void ChromeSecurityStateModelClient::VisibleSecurityStateChanged() {
  if (logged_http_warning_on_current_navigation_)
    return;

  security_state::SecurityStateModel::SecurityInfo security_info;
  GetSecurityInfo(&security_info);
  if (!security_info.displayed_private_user_data_input_on_http)
    return;

  std::string warning;
  switch (security_info.security_level) {
    case security_state::SecurityStateModel::HTTP_SHOW_WARNING:
      warning =
          "This page includes a password or credit card input in a non-secure "
          "context. A warning has been added to the URL bar. For more "
          "information, see https://goo.gl/zmWq3m.";
      break;
    case security_state::SecurityStateModel::NONE:
      warning =
          "This page includes a password or credit card input in a non-secure "
          "context. A warning will be added to the URL bar in Chrome 56 (Jan "
          "2017). For more information, see https://goo.gl/zmWq3m.";
      break;
    default:
      return;
  }

  logged_http_warning_on_current_navigation_ = true;
  web_contents_->GetMainFrame()->AddMessageToConsole(
      content::CONSOLE_MESSAGE_LEVEL_WARNING, warning);
}

void ChromeSecurityStateModelClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSynchronousNavigation()) {
    // Only reset the console message flag for main-frame navigations,
    // and not for synchronous navigations like reference fragments and
    // pushState.
    logged_http_warning_on_current_navigation_ = false;
  }
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
  if (!entry) {
    *state = SecurityStateModel::VisibleSecurityState();
    return;
  }

  if (!entry->GetSSL().initialized) {
    *state = SecurityStateModel::VisibleSecurityState();
    // Connection security information is still being initialized, but malware
    // status might already be known.
    CheckSafeBrowsingStatus(entry, web_contents_, state);
    return;
  }

  state->connection_info_initialized = true;
  state->url = entry->GetURL();
  const content::SSLStatus& ssl = entry->GetSSL();
  state->certificate = ssl.certificate;
  state->cert_status = ssl.cert_status;
  state->connection_status = ssl.connection_status;
  state->key_exchange_group = ssl.key_exchange_group;
  state->security_bits = ssl.security_bits;
  state->pkp_bypassed = ssl.pkp_bypassed;
  state->sct_verify_statuses.clear();
  state->sct_verify_statuses.insert(state->sct_verify_statuses.begin(),
                                    ssl.sct_statuses.begin(),
                                    ssl.sct_statuses.end());
  state->displayed_mixed_content =
      !!(ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  state->ran_mixed_content =
      !!(ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT);
  state->displayed_content_with_cert_errors =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
  state->ran_content_with_cert_errors =
      !!(ssl.content_status & content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
  state->displayed_password_field_on_http =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  state->displayed_credit_card_field_on_http =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);

  CheckSafeBrowsingStatus(entry, web_contents_, state);
}
