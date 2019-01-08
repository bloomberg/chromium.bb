// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif  // defined(OS_CHROMEOS)

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

namespace {

void RecordSecurityLevel(const security_state::SecurityInfo& security_info) {
  if (security_info.scheme_is_cryptographic) {
    UMA_HISTOGRAM_ENUMERATION("Security.SecurityLevel.CryptographicScheme",
                              security_info.security_level,
                              security_state::SECURITY_LEVEL_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Security.SecurityLevel.NoncryptographicScheme",
                              security_info.security_level,
                              security_state::SECURITY_LEVEL_COUNT);
  }
}

bool IsOriginSecureWithWhitelist(
    const std::vector<std::string>& secure_origins_and_patterns,
    const GURL& url) {
  if (content::IsOriginSecure(url))
    return true;

  url::Origin origin = url::Origin::Create(url);
  if (base::ContainsValue(secure_origins_and_patterns, origin.Serialize()))
    return true;
  for (const auto& origin_or_pattern : secure_origins_and_patterns) {
    if (base::MatchPattern(origin.host(), origin_or_pattern))
      return true;
  }
  return false;
}

}  // namespace

using safe_browsing::SafeBrowsingUIManager;

SecurityStateTabHelper::SecurityStateTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

SecurityStateTabHelper::~SecurityStateTabHelper() {}

void SecurityStateTabHelper::GetSecurityInfo(
    security_state::SecurityInfo* result) const {
  security_state::GetSecurityInfo(
      GetVisibleSecurityState(), UsedPolicyInstalledCertificate(),
      base::BindRepeating(&IsOriginSecureWithWhitelist,
                          GetSecureOriginsAndPatterns()),
      result);
}

void SecurityStateTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsFormSubmission()) {
    security_state::SecurityInfo info;
    GetSecurityInfo(&info);
    UMA_HISTOGRAM_ENUMERATION("Security.SecurityLevel.FormSubmission",
                              info.security_level,
                              security_state::SECURITY_LEVEL_COUNT);
  }
}

void SecurityStateTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe navigations, same-document navigations, and navigations
  // that did not commit (e.g. HTTP/204 or file downloads).
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (entry) {
    UMA_HISTOGRAM_ENUMERATION(
        "Security.CertificateTransparency.MainFrameNavigationCompliance",
        entry->GetSSL().ct_policy_compliance,
        net::ct::CTPolicyCompliance::CT_POLICY_COUNT);
  }

  security_state::SecurityInfo security_info;
  GetSecurityInfo(&security_info);
  if (net::IsCertStatusError(security_info.cert_status) &&
      !net::IsCertStatusMinorError(security_info.cert_status) &&
      !navigation_handle->IsErrorPage()) {
    // Record each time a user visits a site after having clicked through a
    // certificate warning interstitial. This is used as a baseline for
    // interstitial.ssl.did_user_revoke_decision2 in order to determine how
    // many times the re-enable warnings button is clicked, as a fraction of
    // the number of times it was available.
    UMA_HISTOGRAM_BOOLEAN("interstitial.ssl.visited_site_after_warning", true);
  }

  // Security indicator UI study (https://crbug.com/803501): Show a message in
  // the console to reduce developer confusion about the experimental UI
  // treatments for HTTPS pages with EV certificates.
  const std::string parameter =
      base::FeatureList::IsEnabled(omnibox::kSimplifyHttpsIndicator)
          ? base::GetFieldTrialParamValueByFeature(
                omnibox::kSimplifyHttpsIndicator,
                OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterName)
          : std::string();
  if (security_info.security_level == security_state::EV_SECURE) {
    if (parameter ==
        OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterEvToSecure) {
      web_contents()->GetMainFrame()->AddMessageToConsole(
          content::CONSOLE_MESSAGE_LEVEL_INFO,
          "As part of an experiment, Chrome temporarily shows only the "
          "\"Secure\" text in the address bar. Your SSL certificate with "
          "Extended Validation is still valid.");
    }
    if (parameter ==
        OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterBothToLock) {
      web_contents()->GetMainFrame()->AddMessageToConsole(
          content::CONSOLE_MESSAGE_LEVEL_INFO,
          "As part of an experiment, Chrome temporarily shows only the lock "
          "icon in the address bar. Your SSL certificate with Extended "
          "Validation is still valid.");
    }
  }
}

void SecurityStateTabHelper::DidChangeVisibleSecurityState() {
  security_state::SecurityInfo security_info;
  GetSecurityInfo(&security_info);
  RecordSecurityLevel(security_info);
}

bool SecurityStateTabHelper::UsedPolicyInstalledCertificate() const {
#if defined(OS_CHROMEOS)
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (service && service->UsedPolicyCertificates())
    return true;
#endif
  return false;
}

security_state::MaliciousContentStatus
SecurityStateTabHelper::GetMaliciousContentStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return security_state::MALICIOUS_CONTENT_STATUS_NONE;
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service)
    return security_state::MALICIOUS_CONTENT_STATUS_NONE;
  scoped_refptr<SafeBrowsingUIManager> sb_ui_manager = sb_service->ui_manager();
  safe_browsing::SBThreatType threat_type;
  if (sb_ui_manager->IsUrlWhitelistedOrPendingForWebContents(
          entry->GetURL(), false, entry, web_contents(), false, &threat_type)) {
    switch (threat_type) {
      case safe_browsing::SB_THREAT_TYPE_UNUSED:
      case safe_browsing::SB_THREAT_TYPE_SAFE:
      case safe_browsing::SB_THREAT_TYPE_URL_PHISHING:
      case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
        return security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
      case safe_browsing::SB_THREAT_TYPE_URL_MALWARE:
      case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
        return security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
      case safe_browsing::SB_THREAT_TYPE_URL_UNWANTED:
        return security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE;
      case safe_browsing::SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE:
#if defined(SAFE_BROWSING_DB_LOCAL)
        if (safe_browsing::ChromePasswordProtectionService::
                ShouldShowPasswordReusePageInfoBubble(
                    web_contents(),
                    safe_browsing::LoginReputationClientRequest::
                        PasswordReuseEvent::SIGN_IN_PASSWORD)) {
          return security_state::
              MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE;
        }
        // If user has already changed Gaia password, returns the regular
        // social engineering content status.
        return security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
#endif
      case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
#if defined(SAFE_BROWSING_DB_LOCAL)
        if (safe_browsing::ChromePasswordProtectionService::
                ShouldShowPasswordReusePageInfoBubble(
                    web_contents(),
                    safe_browsing::LoginReputationClientRequest::
                        PasswordReuseEvent::ENTERPRISE_PASSWORD)) {
          return security_state::
              MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE;
        }
        // If user has already changed Gaia password, returns the regular
        // social engineering content status.
        return security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
#endif
      case safe_browsing::SB_THREAT_TYPE_BILLING:
        return base::FeatureList::IsEnabled(safe_browsing::kBillingInterstitial)
                   ? security_state::MALICIOUS_CONTENT_STATUS_BILLING
                   : security_state::MALICIOUS_CONTENT_STATUS_NONE;
      case safe_browsing::
          DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
      case safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE:
      case safe_browsing::SB_THREAT_TYPE_EXTENSION:
      case safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE:
      case safe_browsing::SB_THREAT_TYPE_API_ABUSE:
      case safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER:
      case safe_browsing::SB_THREAT_TYPE_CSD_WHITELIST:
      case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
      case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
      case safe_browsing::SB_THREAT_TYPE_APK_DOWNLOAD:
        // These threat types are not currently associated with
        // interstitials, and thus resources with these threat types are
        // not ever whitelisted or pending whitelisting.
        NOTREACHED();
        break;
    }
  }
  return security_state::MALICIOUS_CONTENT_STATUS_NONE;
}

std::unique_ptr<security_state::VisibleSecurityState>
SecurityStateTabHelper::GetVisibleSecurityState() const {
  auto state = security_state::GetVisibleSecurityState(web_contents());

  // Malware status might already be known even if connection security
  // information is still being initialized, thus no need to check for that.
  state->malicious_content_status = GetMaliciousContentStatus();

  return state;
}

std::vector<std::string> SecurityStateTabHelper::GetSecureOriginsAndPatterns()
    const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  std::string origins_str = "";
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure)) {
    origins_str = command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure);
  } else if (prefs->HasPrefPath(prefs::kUnsafelyTreatInsecureOriginAsSecure)) {
    origins_str = prefs->GetString(prefs::kUnsafelyTreatInsecureOriginAsSecure);
  }
  return secure_origin_whitelist::ParseWhitelist(origins_str);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SecurityStateTabHelper)
