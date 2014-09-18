// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ui/website_settings/website_settings_infobar_delegate.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
using content::BrowserThread;

namespace {

// Events for UMA. Do not reorder or change!
enum SSLCertificateDecisionsDidRevoke {
  USER_CERT_DECISIONS_NOT_REVOKED = 0,
  USER_CERT_DECISIONS_REVOKED,
  END_OF_SSL_CERTIFICATE_DECISIONS_DID_REVOKE_ENUM
};

// The list of content settings types to display on the Website Settings UI.
ContentSettingsType kPermissionType[] = {
  CONTENT_SETTINGS_TYPE_IMAGES,
  CONTENT_SETTINGS_TYPE_JAVASCRIPT,
  CONTENT_SETTINGS_TYPE_PLUGINS,
  CONTENT_SETTINGS_TYPE_POPUPS,
  CONTENT_SETTINGS_TYPE_GEOLOCATION,
  CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
  CONTENT_SETTINGS_TYPE_FULLSCREEN,
  CONTENT_SETTINGS_TYPE_MOUSELOCK,
  CONTENT_SETTINGS_TYPE_MEDIASTREAM,
  CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
  CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
};

bool CertificateTransparencyStatusMatch(
    const content::SignedCertificateTimestampIDStatusList& scts,
    net::ct::SCTVerifyStatus status) {
  for (content::SignedCertificateTimestampIDStatusList::const_iterator it =
           scts.begin();
       it != scts.end();
       ++it) {
    if (it->status == status)
      return true;
  }

  return false;
}

int GetSiteIdentityDetailsMessageByCTInfo(
    const content::SignedCertificateTimestampIDStatusList& scts,
    bool is_ev) {
  // No SCTs - no CT information.
  if (scts.empty())
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_NO_CT
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_NO_CT);

  if (CertificateTransparencyStatusMatch(scts, net::ct::SCT_STATUS_OK))
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_VERIFIED
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_VERIFIED);

  if (CertificateTransparencyStatusMatch(scts, net::ct::SCT_STATUS_INVALID))
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_INVALID
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_INVALID);

  // status is SCT_STATUS_LOG_UNKNOWN
  return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_UNVERIFIED
                : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_UNVERIFIED);
}

// This function will return SITE_IDENTITY_STATUS_CERT or
// SITE_IDENTITY_STATUS_EV_CERT depending on |is_ev| unless there are SCTs
// which failed verification, in which case it will return
// SITE_IDENTITY_STATUS_ERROR.
WebsiteSettings::SiteIdentityStatus GetSiteIdentityStatusByCTInfo(
    const content::SignedCertificateTimestampIDStatusList& scts,
    bool is_ev) {
  if (CertificateTransparencyStatusMatch(scts, net::ct::SCT_STATUS_INVALID))
    return WebsiteSettings::SITE_IDENTITY_STATUS_ERROR;

  return is_ev ? WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT
               : WebsiteSettings::SITE_IDENTITY_STATUS_CERT;
}

const char kRememberCertificateErrorDecisionsFieldTrialName[] =
    "RememberCertificateErrorDecisions";
const char kRememberCertificateErrorDecisionsFieldTrialDefaultGroup[] =
    "Default";
const char kRememberCertificateErrorDecisionsFieldTrialDisableGroup[] =
    "Disable";
// Returns true if the user is in the experimental group or has the flag enabled
// for remembering SSL error decisions, otherwise false.
//
// TODO(jww): The field trial is scheduled to end 2015/02/28. This should be
// removed at that point unless the field trial or flag continues.
bool InRememberCertificateErrorDecisionsGroup() {
  std::string group_name = base::FieldTrialList::FindFullName(
      kRememberCertificateErrorDecisionsFieldTrialName);

  // The Default and Disable groups are the "old-style" forget-at-session
  // restart groups, so they do not get the button.
  bool in_experimental_group = !group_name.empty() &&
      group_name.compare(
          kRememberCertificateErrorDecisionsFieldTrialDefaultGroup) != 0 &&
      group_name.compare(
          kRememberCertificateErrorDecisionsFieldTrialDisableGroup) != 0;
  bool has_command_line_switch = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRememberCertErrorDecisions);
  return in_experimental_group || has_command_line_switch;
}

}  // namespace

WebsiteSettings::WebsiteSettings(
    WebsiteSettingsUI* ui,
    Profile* profile,
    TabSpecificContentSettings* tab_specific_content_settings,
    InfoBarService* infobar_service,
    const GURL& url,
    const content::SSLStatus& ssl,
    content::CertStore* cert_store)
    : TabSpecificContentSettings::SiteDataObserver(
          tab_specific_content_settings),
      ui_(ui),
      infobar_service_(infobar_service),
      show_info_bar_(false),
      site_url_(url),
      site_identity_status_(SITE_IDENTITY_STATUS_UNKNOWN),
      cert_id_(0),
      site_connection_status_(SITE_CONNECTION_STATUS_UNKNOWN),
      cert_store_(cert_store),
      content_settings_(profile->GetHostContentSettingsMap()),
      chrome_ssl_host_state_delegate_(
          ChromeSSLHostStateDelegateFactory::GetForProfile(profile)),
      did_revoke_user_ssl_decisions_(false) {
  Init(profile, url, ssl);

  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  if (history_service) {
    history_service->GetVisibleVisitCountToHost(
        site_url_,
        base::Bind(&WebsiteSettings::OnGotVisitCountToHost,
                   base::Unretained(this)),
        &visit_count_task_tracker_);
  }

  PresentSitePermissions();
  PresentSiteData();
  PresentSiteIdentity();
  PresentHistoryInfo(base::Time());

  // Every time the Website Settings UI is opened a |WebsiteSettings| object is
  // created. So this counts how ofter the Website Settings UI is opened.
  content::RecordAction(base::UserMetricsAction("WebsiteSettings_Opened"));
}

WebsiteSettings::~WebsiteSettings() {
}

void WebsiteSettings::OnSitePermissionChanged(ContentSettingsType type,
                                              ContentSetting setting) {
  // Count how often a permission for a specific content type is changed using
  // the Website Settings UI.
  UMA_HISTOGRAM_COUNTS("WebsiteSettings.PermissionChanged", type);

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      // TODO(markusheintz): The rule we create here should also change the
      // location permission for iframed content.
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
      secondary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      break;
    case CONTENT_SETTINGS_TYPE_IMAGES:
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
    case CONTENT_SETTINGS_TYPE_PLUGINS:
    case CONTENT_SETTINGS_TYPE_POPUPS:
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      primary_pattern = ContentSettingsPattern::FromURL(site_url_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM: {
      // We need to use the same same patterns as other places like infobar code
      // to override the existing rule instead of creating the new one.
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      // Set permission for both microphone and camera.
      content_settings_->SetContentSetting(
          primary_pattern,
          secondary_pattern,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          std::string(),
          setting);

      content_settings_->SetContentSetting(
          primary_pattern,
          secondary_pattern,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          std::string(),
          setting);
      break;
    }
    default:
      NOTREACHED() << "ContentSettingsType " << type << "is not supported.";
      break;
  }

  if (type != CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
    // Permission settings are specified via rules. There exists always at least
    // one rule for the default setting. Get the rule that currently defines
    // the permission for the given permission |type|. Then test whether the
    // existing rule is more specific than the rule we are about to create. If
    // the existing rule is more specific, than change the existing rule instead
    // of creating a new rule that would be hidden behind the existing rule.
    // This is not a concern for CONTENT_SETTINGS_TYPE_MEDIASTREAM since users
    // can not create media settings exceptions by hand.
    content_settings::SettingInfo info;
    scoped_ptr<base::Value> v =
        content_settings_->GetWebsiteSettingWithoutOverride(
            site_url_, site_url_, type, std::string(), &info);
    content_settings_->SetNarrowestWebsiteSetting(
        primary_pattern, secondary_pattern, type, std::string(), setting, info);
  } else {
    base::Value* value = NULL;
    if (setting != CONTENT_SETTING_DEFAULT)
      value = new base::FundamentalValue(setting);
    content_settings_->SetWebsiteSetting(
        primary_pattern, secondary_pattern, type, std::string(), value);
  }

  show_info_bar_ = true;

// TODO(markusheintz): This is a temporary hack to fix issue:
// http://crbug.com/144203.
#if defined(OS_MACOSX)
  // Refresh the UI to reflect the new setting.
  PresentSitePermissions();
#endif
}

void WebsiteSettings::OnGotVisitCountToHost(bool found_visits,
                                            int visit_count,
                                            base::Time first_visit) {
  if (!found_visits) {
    // This indicates an error, such as the page's URL scheme wasn't
    // http/https.
    first_visit = base::Time();
  } else if (visit_count == 0) {
    first_visit = base::Time::Now();
  }
  PresentHistoryInfo(first_visit);
}

void WebsiteSettings::OnSiteDataAccessed() {
  PresentSiteData();
}

void WebsiteSettings::OnUIClosing() {
  if (show_info_bar_)
    WebsiteSettingsInfoBarDelegate::Create(infobar_service_);

  SSLCertificateDecisionsDidRevoke user_decision =
      did_revoke_user_ssl_decisions_ ? USER_CERT_DECISIONS_REVOKED
                                     : USER_CERT_DECISIONS_NOT_REVOKED;

  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.did_user_revoke_decisions",
                            user_decision,
                            END_OF_SSL_CERTIFICATE_DECISIONS_DID_REVOKE_ENUM);
}

void WebsiteSettings::OnRevokeSSLErrorBypassButtonPressed() {
  DCHECK(chrome_ssl_host_state_delegate_);
  chrome_ssl_host_state_delegate_->RevokeUserAllowExceptionsHard(
      site_url().host());
  did_revoke_user_ssl_decisions_ = true;
}

void WebsiteSettings::Init(Profile* profile,
                           const GURL& url,
                           const content::SSLStatus& ssl) {
  if (url.SchemeIs(content::kChromeUIScheme)) {
    site_identity_status_ = SITE_IDENTITY_STATUS_INTERNAL_PAGE;
    site_identity_details_ =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE);
    site_connection_status_ = SITE_CONNECTION_STATUS_INTERNAL_PAGE;
    return;
  }

  scoped_refptr<net::X509Certificate> cert;

  // Identity section.
  base::string16 subject_name(UTF8ToUTF16(url.host()));
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
  }

  cert_id_ = ssl.cert_id;

  if (ssl.cert_id && !ssl.signed_certificate_timestamp_ids.empty()) {
    signed_certificate_timestamp_ids_.assign(
        ssl.signed_certificate_timestamp_ids.begin(),
        ssl.signed_certificate_timestamp_ids.end());
  }

  if (ssl.cert_id &&
      cert_store_->RetrieveCert(ssl.cert_id, &cert) &&
      (!net::IsCertStatusError(ssl.cert_status) ||
       net::IsCertStatusMinorError(ssl.cert_status))) {
    // There are no major errors. Check for minor errors.
#if defined(OS_CHROMEOS)
    policy::PolicyCertService* service =
        policy::PolicyCertServiceFactory::GetForProfile(profile);
    const bool used_policy_certs = service && service->UsedPolicyCertificates();
#else
    const bool used_policy_certs = false;
#endif
    if (used_policy_certs) {
      site_identity_status_ = SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      site_identity_details_ = l10n_util::GetStringFUTF16(
          IDS_CERT_POLICY_PROVIDED_CERT_MESSAGE, UTF8ToUTF16(url.host()));
    } else if (net::IsCertStatusMinorError(ssl.cert_status)) {
      site_identity_status_ = SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN;
      base::string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }

      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          GetSiteIdentityDetailsMessageByCTInfo(
              ssl.signed_certificate_timestamp_ids, false /* not EV */),
          issuer_name));

      site_identity_details_ += ASCIIToUTF16("\n\n");
      if (ssl.cert_status & net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION) {
        site_identity_details_ += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNABLE_TO_CHECK_REVOCATION);
      } else if (ssl.cert_status & net::CERT_STATUS_NO_REVOCATION_MECHANISM) {
        site_identity_details_ += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_NO_REVOCATION_MECHANISM);
      } else {
        NOTREACHED() << "Need to specify string for this warning";
      }
    } else if (ssl.cert_status & net::CERT_STATUS_IS_EV) {
      // EV HTTPS page.
      site_identity_status_ = GetSiteIdentityStatusByCTInfo(
          ssl.signed_certificate_timestamp_ids, true);
      DCHECK(!cert->subject().organization_names.empty());
      organization_name_ = UTF8ToUTF16(cert->subject().organization_names[0]);
      // An EV Cert is required to have a city (localityName) and country but
      // state is "if any".
      DCHECK(!cert->subject().locality_name.empty());
      DCHECK(!cert->subject().country_name.empty());
      base::string16 locality;
      if (!cert->subject().state_or_province_name.empty()) {
        locality = l10n_util::GetStringFUTF16(
            IDS_PAGEINFO_ADDRESS,
            UTF8ToUTF16(cert->subject().locality_name),
            UTF8ToUTF16(cert->subject().state_or_province_name),
            UTF8ToUTF16(cert->subject().country_name));
      } else {
        locality = l10n_util::GetStringFUTF16(
            IDS_PAGEINFO_PARTIAL_ADDRESS,
            UTF8ToUTF16(cert->subject().locality_name),
            UTF8ToUTF16(cert->subject().country_name));
      }
      DCHECK(!cert->subject().organization_names.empty());
      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          GetSiteIdentityDetailsMessageByCTInfo(
              ssl.signed_certificate_timestamp_ids, true /* is EV */),
          UTF8ToUTF16(cert->subject().organization_names[0]),
          locality,
          UTF8ToUTF16(cert->issuer().GetDisplayName())));
    } else {
      // Non-EV OK HTTPS page.
      site_identity_status_ = GetSiteIdentityStatusByCTInfo(
          ssl.signed_certificate_timestamp_ids, false);
      base::string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }

      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          GetSiteIdentityDetailsMessageByCTInfo(
              ssl.signed_certificate_timestamp_ids, false /* not EV */),
          issuer_name));
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    site_identity_details_.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    if (ssl.security_style == content::SECURITY_STYLE_UNAUTHENTICATED)
      site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
    else
      site_identity_status_ = SITE_IDENTITY_STATUS_ERROR;

    const base::string16 bullet = UTF8ToUTF16("\n • ");
    std::vector<SSLErrorInfo> errors;
    SSLErrorInfo::GetErrorsForCertStatus(ssl.cert_id, ssl.cert_status,
                                         url, &errors);
    for (size_t i = 0; i < errors.size(); ++i) {
      site_identity_details_ += bullet;
      site_identity_details_ += errors[i].short_description();
    }

    if (ssl.cert_status & net::CERT_STATUS_NON_UNIQUE_NAME) {
      site_identity_details_ += ASCIIToUTF16("\n\n");
      site_identity_details_ += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NON_UNIQUE_NAME);
    }
  }

  // Site Connection
  // We consider anything less than 80 bits encryption to be weak encryption.
  // TODO(wtc): Bug 1198735: report mixed/unsafe content for unencrypted and
  // weakly encrypted connections.
  site_connection_status_ = SITE_CONNECTION_STATUS_UNKNOWN;

  if (ssl.security_style == content::SECURITY_STYLE_UNKNOWN) {
    // Page is still loading, so SSL status is not yet available. Say nothing.
    DCHECK_EQ(ssl.security_bits, -1);
    site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;

    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (ssl.security_style == content::SECURITY_STYLE_UNAUTHENTICATED) {
    // HTTPS without a certificate, or not HTTPS.
    DCHECK(!ssl.cert_id);
    site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;

    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (ssl.security_bits < 0) {
    // Security strength is unknown.  Say nothing.
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
  } else if (ssl.security_bits == 0) {
    DCHECK_NE(ssl.security_style, content::SECURITY_STYLE_UNAUTHENTICATED);
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (ssl.security_bits < 80) {
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
        subject_name));
  } else {
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED;
    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
        subject_name,
        base::IntToString16(ssl.security_bits)));
    if (ssl.content_status) {
      bool ran_insecure_content =
          !!(ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT);
      site_connection_status_ = ran_insecure_content ?
          SITE_CONNECTION_STATUS_ENCRYPTED_ERROR
          : SITE_CONNECTION_STATUS_MIXED_CONTENT;
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
          site_connection_details_,
          l10n_util::GetStringUTF16(ran_insecure_content ?
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_ERROR :
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING)));
    }
  }

  uint16 cipher_suite =
      net::SSLConnectionStatusToCipherSuite(ssl.connection_status);
  if (ssl.security_bits > 0 && cipher_suite) {
    int ssl_version =
        net::SSLConnectionStatusToVersion(ssl.connection_status);
    const char* ssl_version_str;
    net::SSLVersionToString(&ssl_version_str, ssl_version);
    site_connection_details_ += ASCIIToUTF16("\n\n");
    site_connection_details_ += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_SSL_VERSION,
        ASCIIToUTF16(ssl_version_str));

    bool did_fallback = (ssl.connection_status &
                         net::SSL_CONNECTION_VERSION_FALLBACK) != 0;
    bool no_renegotiation =
        (ssl.connection_status &
        net::SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION) != 0;
    const char *key_exchange, *cipher, *mac;
    bool is_aead;
    net::SSLCipherSuiteToStrings(
        &key_exchange, &cipher, &mac, &is_aead, cipher_suite);

    site_connection_details_ += ASCIIToUTF16("\n\n");
    if (is_aead) {
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS_AEAD,
          ASCIIToUTF16(cipher), ASCIIToUTF16(key_exchange));
    } else {
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS,
          ASCIIToUTF16(cipher), ASCIIToUTF16(mac), ASCIIToUTF16(key_exchange));
    }

    if (did_fallback) {
      // For now, only SSLv3 fallback will trigger a warning icon.
      if (site_connection_status_ < SITE_CONNECTION_STATUS_MIXED_CONTENT)
        site_connection_status_ = SITE_CONNECTION_STATUS_MIXED_CONTENT;
      site_connection_details_ += ASCIIToUTF16("\n\n");
      site_connection_details_ += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_FALLBACK_MESSAGE);
    }
    if (no_renegotiation) {
      site_connection_details_ += ASCIIToUTF16("\n\n");
      site_connection_details_ += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_RENEGOTIATION_MESSAGE);
    }
  }

  // Check if a user decision has been made to allow or deny certificates with
  // errors on this site.
  ChromeSSLHostStateDelegate* delegate =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile);
  DCHECK(delegate);
  // Only show an SSL decision revoke button if both the user has chosen to
  // bypass SSL host errors for this host in the past and the user is not using
  // the traditional "forget-at-session-restart" error decision memory.
  show_ssl_decision_revoke_button_ = delegate->HasAllowException(url.host()) &&
                                     InRememberCertificateErrorDecisionsGroup();

  // By default select the permissions tab that displays all the site
  // permissions. In case of a connection error or an issue with the
  // certificate presented by the website, select the connection tab to draw
  // the user's attention to the issue. If the site does not provide a
  // certificate because it was loaded over an unencrypted connection, don't
  // select the connection tab.
  WebsiteSettingsUI::TabId tab_id = WebsiteSettingsUI::TAB_ID_PERMISSIONS;
  if (site_connection_status_ == SITE_CONNECTION_STATUS_ENCRYPTED_ERROR ||
      site_connection_status_ == SITE_CONNECTION_STATUS_MIXED_CONTENT ||
      site_identity_status_ == SITE_IDENTITY_STATUS_ERROR ||
      site_identity_status_ == SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN ||
      site_identity_status_ == SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT)
    tab_id = WebsiteSettingsUI::TAB_ID_CONNECTION;
  ui_->SetSelectedTab(tab_id);
}

void WebsiteSettings::PresentSitePermissions() {
  PermissionInfoList permission_info_list;

  WebsiteSettingsUI::PermissionInfo permission_info;
  for (size_t i = 0; i < arraysize(kPermissionType); ++i) {
    permission_info.type = kPermissionType[i];
    if (permission_info.type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
      const CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (!command_line->HasSwitch(switches::kEnableWebMIDI))
        continue;
    }

    content_settings::SettingInfo info;
    if (permission_info.type == CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
      scoped_ptr<base::Value> mic_value =
          content_settings_->GetWebsiteSettingWithoutOverride(
              site_url_,
              site_url_,
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
              std::string(),
              &info);
      ContentSetting mic_setting =
          content_settings::ValueToContentSetting(mic_value.get());

      scoped_ptr<base::Value> camera_value =
          content_settings_->GetWebsiteSettingWithoutOverride(
              site_url_,
              site_url_,
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
              std::string(),
              &info);
      ContentSetting camera_setting =
          content_settings::ValueToContentSetting(camera_value.get());

      if (mic_setting != camera_setting || mic_setting == CONTENT_SETTING_ASK)
        permission_info.setting = CONTENT_SETTING_DEFAULT;
      else
        permission_info.setting = mic_setting;
    } else {
      scoped_ptr<base::Value> value =
          content_settings_->GetWebsiteSettingWithoutOverride(
              site_url_, site_url_, permission_info.type, std::string(), &info);
      DCHECK(value.get());
      if (value->GetType() == base::Value::TYPE_INTEGER) {
        permission_info.setting =
            content_settings::ValueToContentSetting(value.get());
      } else {
        NOTREACHED();
      }
    }

    permission_info.source = info.source;

    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard() &&
        permission_info.type != CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
      permission_info.default_setting = permission_info.setting;
      permission_info.setting = CONTENT_SETTING_DEFAULT;
    } else {
      permission_info.default_setting =
          content_settings_->GetDefaultContentSetting(permission_info.type,
                                                      NULL);
    }
    permission_info_list.push_back(permission_info);
  }

  ui_->SetPermissionInfo(permission_info_list);
}

void WebsiteSettings::PresentSiteData() {
  CookieInfoList cookie_info_list;
  const LocalSharedObjectsContainer& allowed_objects =
      tab_specific_content_settings()->allowed_local_shared_objects();
  const LocalSharedObjectsContainer& blocked_objects =
      tab_specific_content_settings()->blocked_local_shared_objects();

  // Add first party cookie and site data counts.
  WebsiteSettingsUI::CookieInfo cookie_info;
  std::string cookie_source =
      net::registry_controlled_domains::GetDomainAndRegistry(
          site_url_,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (cookie_source.empty())
    cookie_source = site_url_.host();
  cookie_info.cookie_source = cookie_source;
  cookie_info.allowed = allowed_objects.GetObjectCountForDomain(site_url_);
  cookie_info.blocked = blocked_objects.GetObjectCountForDomain(site_url_);
  cookie_info_list.push_back(cookie_info);

  // Add third party cookie counts.
  cookie_info.cookie_source = l10n_util::GetStringUTF8(
     IDS_WEBSITE_SETTINGS_THIRD_PARTY_SITE_DATA);
  cookie_info.allowed = allowed_objects.GetObjectCount() - cookie_info.allowed;
  cookie_info.blocked = blocked_objects.GetObjectCount() - cookie_info.blocked;
  cookie_info_list.push_back(cookie_info);

  ui_->SetCookieInfo(cookie_info_list);
}

void WebsiteSettings::PresentSiteIdentity() {
  // After initialization the status about the site's connection
  // and it's identity must be available.
  DCHECK_NE(site_identity_status_, SITE_IDENTITY_STATUS_UNKNOWN);
  DCHECK_NE(site_connection_status_, SITE_CONNECTION_STATUS_UNKNOWN);
  WebsiteSettingsUI::IdentityInfo info;
  if (site_identity_status_ == SITE_IDENTITY_STATUS_EV_CERT)
    info.site_identity = UTF16ToUTF8(organization_name());
  else
    info.site_identity = site_url_.host();

  info.connection_status = site_connection_status_;
  info.connection_status_description =
      UTF16ToUTF8(site_connection_details_);
  info.identity_status = site_identity_status_;
  info.identity_status_description =
      UTF16ToUTF8(site_identity_details_);
  info.cert_id = cert_id_;
  info.signed_certificate_timestamp_ids.assign(
      signed_certificate_timestamp_ids_.begin(),
      signed_certificate_timestamp_ids_.end());
  info.show_ssl_decision_revoke_button = show_ssl_decision_revoke_button_;
  ui_->SetIdentityInfo(info);
}

void WebsiteSettings::PresentHistoryInfo(base::Time first_visit) {
  if (first_visit == base::Time()) {
    ui_->SetFirstVisit(base::string16());
    return;
  }

  bool visited_before_today = false;
  base::Time today = base::Time::Now().LocalMidnight();
  base::Time first_visit_midnight = first_visit.LocalMidnight();
  visited_before_today = (first_visit_midnight < today);

  base::string16 first_visit_text;
  if (visited_before_today) {
    first_visit_text = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_VISITED_BEFORE_TODAY,
        base::TimeFormatShortDate(first_visit));
  } else {
    first_visit_text = l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_FIRST_VISITED_TODAY);
  }
  ui_->SetFirstVisit(first_visit_text);
}
