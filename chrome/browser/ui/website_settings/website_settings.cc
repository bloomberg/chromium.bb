// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/local_shared_objects_counter.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/rappor/rappor_utils.h"
#include "components/ssl_errors/error_info.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/website_settings/website_settings_infobar_delegate.h"
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

// The list of content settings types to display on the Website Settings UI. THE
// ORDER OF THESE ITEMS IS IMPORTANT. To propose changing it, email
// security-dev@chromium.org.
ContentSettingsType kPermissionType[] = {
    CONTENT_SETTINGS_TYPE_GEOLOCATION,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
    CONTENT_SETTINGS_TYPE_IMAGES,
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
    CONTENT_SETTINGS_TYPE_POPUPS,
    CONTENT_SETTINGS_TYPE_FULLSCREEN,
    CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTINGS_TYPE_MOUSELOCK,
    CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
#if defined(OS_ANDROID)
    CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
#endif
};

bool CertificateTransparencyStatusMatch(
    const std::vector<net::ct::SCTVerifyStatus>& sct_verify_statuses,
    net::ct::SCTVerifyStatus status) {
  for (const auto& verify_status : sct_verify_statuses) {
    if (verify_status == status)
      return true;
  }
  return false;
}

int GetSiteIdentityDetailsMessageByCTInfo(
    const std::vector<net::ct::SCTVerifyStatus>& sct_verify_statuses,
    bool is_ev) {
  // No SCTs - no CT information.
  if (sct_verify_statuses.empty())
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_NO_CT
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_NO_CT);

  if (CertificateTransparencyStatusMatch(sct_verify_statuses,
                                         net::ct::SCT_STATUS_OK))
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_VERIFIED
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_VERIFIED);

  if (CertificateTransparencyStatusMatch(sct_verify_statuses,
                                         net::ct::SCT_STATUS_INVALID))
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
    const std::vector<net::ct::SCTVerifyStatus>& sct_verify_statuses,
    bool is_ev) {
  if (CertificateTransparencyStatusMatch(sct_verify_statuses,
                                         net::ct::SCT_STATUS_INVALID))
    return WebsiteSettings::SITE_IDENTITY_STATUS_ERROR;

  return is_ev ? WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT
               : WebsiteSettings::SITE_IDENTITY_STATUS_CERT;
}

base::string16 GetSimpleSiteName(const GURL& url, Profile* profile) {
  std::string languages;
  if (profile)
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
  return url_formatter::FormatUrlForSecurityDisplayOmitScheme(url, languages);
}

}  // namespace

WebsiteSettings::WebsiteSettings(
    WebsiteSettingsUI* ui,
    Profile* profile,
    TabSpecificContentSettings* tab_specific_content_settings,
    content::WebContents* web_contents,
    const GURL& url,
    const SecurityStateModel::SecurityInfo& security_info,
    content::CertStore* cert_store)
    : TabSpecificContentSettings::SiteDataObserver(
          tab_specific_content_settings),
      ui_(ui),
      web_contents_(web_contents),
      show_info_bar_(false),
      site_url_(url),
      site_identity_status_(SITE_IDENTITY_STATUS_UNKNOWN),
      cert_id_(0),
      site_connection_status_(SITE_CONNECTION_STATUS_UNKNOWN),
      cert_store_(cert_store),
      content_settings_(HostContentSettingsMapFactory::GetForProfile(profile)),
      chrome_ssl_host_state_delegate_(
          ChromeSSLHostStateDelegateFactory::GetForProfile(profile)),
      did_revoke_user_ssl_decisions_(false),
      profile_(profile) {
  Init(url, security_info);

  PresentSitePermissions();
  PresentSiteData();
  PresentSiteIdentity();

  // Every time the Website Settings UI is opened a |WebsiteSettings| object is
  // created. So this counts how ofter the Website Settings UI is opened.
  RecordWebsiteSettingsAction(WEBSITE_SETTINGS_OPENED);
}

WebsiteSettings::~WebsiteSettings() {
}

void WebsiteSettings::RecordWebsiteSettingsAction(
    WebsiteSettingsAction action) {
  UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.Action",
                            action,
                            WEBSITE_SETTINGS_COUNT);

  // Use a separate histogram to record actions if they are done on a page with
  // an HTTPS URL. Note that this *disregards* security status.
  //

  // TODO(palmer): Consider adding a new histogram for
  // GURL::SchemeIsCryptographic. (We don't want to replace this call with a
  // call to that function because we don't want to change the meanings of
  // existing metrics.) This would inform the decision to mark non-secure
  // origins as Dubious or Non-Secure; the overall bug for that is
  // crbug.com/454579.
  if (site_url_.SchemeIs(url::kHttpsScheme)) {
    UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.Action.HttpsUrl",
                              action,
                              WEBSITE_SETTINGS_COUNT);
  }
}

// Get corresponding Rappor Metric. TODO(raymes): This should use the same
// code that's in permission_context_uma_util.cc. Figure out how to do that.
// crbug.com/544745.
const std::string GetRapporMetric(ContentSettingsType permission) {
  std::string permission_str;

  if (permission == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    permission_str = "Geolocation";
  } else if (permission == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    permission_str = "Notifications";
  } else if (permission == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    permission_str = "Mic";
  } else if (permission == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    permission_str = "Camera";
  }

  return base::StringPrintf("ContentSettings.PermissionActions_%s.Revoked.Url",
                            permission_str.c_str());
}

void WebsiteSettings::OnSitePermissionChanged(ContentSettingsType type,
                                              ContentSetting setting) {
  // Count how often a permission for a specific content type is changed using
  // the Website Settings UI.
  size_t num_values;
  int histogram_value = ContentSettingTypeToHistogramValue(type, &num_values);
  UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.OriginInfo.PermissionChanged",
                            histogram_value, num_values);

  if (setting == ContentSetting::CONTENT_SETTING_ALLOW) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebsiteSettings.OriginInfo.PermissionChanged.Allowed", histogram_value,
        num_values);
  } else if (setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebsiteSettings.OriginInfo.PermissionChanged.Blocked", histogram_value,
        num_values);
    // Trigger Rappor sampling if it is a permission revoke action.
    const std::string& rappor_metric = GetRapporMetric(type);
    if (!rappor_metric.empty()) {
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(), rappor_metric, this->site_url_);
    }
  }

  // This is technically redundant given the histogram above, but putting the
  // total count of permission changes in another histogram makes it easier to
  // compare it against other kinds of actions in WebsiteSettings[PopupView].
  RecordWebsiteSettingsAction(WEBSITE_SETTINGS_CHANGED_PERMISSION);

  // TODO(raymes): The scoping here should be a property of ContentSettingsInfo.
  // Make this happen! crbug.com/444742.
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
      type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX ||
      type == CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    // TODO(markusheintz): The rule we create here should also change the
    // location permission for iframed content.
    primary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
    secondary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
  } else if (type == CONTENT_SETTINGS_TYPE_IMAGES ||
             type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
             type == CONTENT_SETTINGS_TYPE_PLUGINS ||
             type == CONTENT_SETTINGS_TYPE_POPUPS ||
             type == CONTENT_SETTINGS_TYPE_MOUSELOCK ||
             type == CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS ||
             type == CONTENT_SETTINGS_TYPE_PUSH_MESSAGING) {
    primary_pattern = ContentSettingsPattern::FromURL(site_url_);
    secondary_pattern = ContentSettingsPattern::Wildcard();
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
             type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA ||
             type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    primary_pattern = ContentSettingsPattern::FromURLNoWildcard(site_url_);
    secondary_pattern = ContentSettingsPattern::Wildcard();
  } else {
    NOTREACHED() << "ContentSettingsType " << type << "is not supported.";
  }

  // Permission settings are specified via rules. There exists always at least
  // one rule for the default setting. Get the rule that currently defines
  // the permission for the given permission |type|. Then test whether the
  // existing rule is more specific than the rule we are about to create. If
  // the existing rule is more specific, than change the existing rule instead
  // of creating a new rule that would be hidden behind the existing rule.
  content_settings::SettingInfo info;
  scoped_ptr<base::Value> v =
      content_settings_->GetWebsiteSetting(
          site_url_, site_url_, type, std::string(), &info);
  content_settings_->SetNarrowestWebsiteSetting(
      primary_pattern, secondary_pattern, type, std::string(), setting, info);

  show_info_bar_ = true;

  // Refresh the UI to reflect the new setting.
  PresentSitePermissions();
}

void WebsiteSettings::OnSiteDataAccessed() {
  PresentSiteData();
}

void WebsiteSettings::OnUIClosing() {
#if defined(OS_ANDROID)
  NOTREACHED();
#else
  if (show_info_bar_ && web_contents_) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_);
    if (infobar_service)
      WebsiteSettingsInfoBarDelegate::Create(infobar_service);
  }

  SSLCertificateDecisionsDidRevoke user_decision =
      did_revoke_user_ssl_decisions_ ? USER_CERT_DECISIONS_REVOKED
                                     : USER_CERT_DECISIONS_NOT_REVOKED;

  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.did_user_revoke_decisions",
                            user_decision,
                            END_OF_SSL_CERTIFICATE_DECISIONS_DID_REVOKE_ENUM);
#endif
}

void WebsiteSettings::OnRevokeSSLErrorBypassButtonPressed() {
  DCHECK(chrome_ssl_host_state_delegate_);
  chrome_ssl_host_state_delegate_->RevokeUserAllowExceptionsHard(
      site_url().host());
  did_revoke_user_ssl_decisions_ = true;
}

void WebsiteSettings::Init(
    const GURL& url,
    const SecurityStateModel::SecurityInfo& security_info) {
  bool isChromeUINativeScheme = false;
#if defined(OS_ANDROID)
  isChromeUINativeScheme = url.SchemeIs(chrome::kChromeUINativeScheme);
#endif

  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(url::kAboutScheme) || isChromeUINativeScheme) {
    site_identity_status_ = SITE_IDENTITY_STATUS_INTERNAL_PAGE;
    site_identity_details_ =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE);
    site_connection_status_ = SITE_CONNECTION_STATUS_INTERNAL_PAGE;
    return;
  }

  // Identity section.
  scoped_refptr<net::X509Certificate> cert;
  cert_id_ = security_info.cert_id;

  // HTTPS with no or minor errors.
  if (security_info.cert_id &&
      cert_store_->RetrieveCert(security_info.cert_id, &cert) &&
      (!net::IsCertStatusError(security_info.cert_status) ||
       net::IsCertStatusMinorError(security_info.cert_status))) {
    // There are no major errors. Check for minor errors.
    if (security_info.security_level ==
        SecurityStateModel::SECURITY_POLICY_WARNING) {
      site_identity_status_ = SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      site_identity_details_ = l10n_util::GetStringFUTF16(
          IDS_CERT_POLICY_PROVIDED_CERT_MESSAGE, UTF8ToUTF16(url.host()));
    } else if (net::IsCertStatusMinorError(security_info.cert_status)) {
      site_identity_status_ = SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN;
      base::string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }

      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          GetSiteIdentityDetailsMessageByCTInfo(
              security_info.sct_verify_statuses, false /* not EV */),
          issuer_name));

      site_identity_details_ += ASCIIToUTF16("\n\n");
      if (security_info.cert_status &
          net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION) {
        site_identity_details_ += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNABLE_TO_CHECK_REVOCATION);
      } else if (security_info.cert_status &
                 net::CERT_STATUS_NO_REVOCATION_MECHANISM) {
        site_identity_details_ += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_NO_REVOCATION_MECHANISM);
      } else {
        NOTREACHED() << "Need to specify string for this warning";
      }
    } else {
      if (security_info.cert_status & net::CERT_STATUS_IS_EV) {
        // EV HTTPS page.
        site_identity_status_ = GetSiteIdentityStatusByCTInfo(
            security_info.sct_verify_statuses, true);
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
                security_info.sct_verify_statuses, true /* is EV */),
            UTF8ToUTF16(cert->subject().organization_names[0]), locality,
            UTF8ToUTF16(cert->issuer().GetDisplayName())));
      } else {
        // Non-EV OK HTTPS page.
        site_identity_status_ = GetSiteIdentityStatusByCTInfo(
            security_info.sct_verify_statuses, false);
        base::string16 issuer_name(
            UTF8ToUTF16(cert->issuer().GetDisplayName()));
        if (issuer_name.empty()) {
          issuer_name.assign(l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
        }

        site_identity_details_.assign(l10n_util::GetStringFUTF16(
            GetSiteIdentityDetailsMessageByCTInfo(
                security_info.sct_verify_statuses, false /* not EV */),
            issuer_name));
      }
      switch (security_info.sha1_deprecation_status) {
        case SecurityStateModel::DEPRECATED_SHA1_MINOR:
          site_identity_status_ =
              SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM_MINOR;
          site_identity_details_ +=
              UTF8ToUTF16("\n\n") +
              l10n_util::GetStringUTF16(
                  IDS_PAGE_INFO_SECURITY_TAB_DEPRECATED_SIGNATURE_ALGORITHM_MINOR);
          break;
        case SecurityStateModel::DEPRECATED_SHA1_MAJOR:
          site_identity_status_ =
              SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM_MAJOR;
          site_identity_details_ +=
              UTF8ToUTF16("\n\n") +
              l10n_util::GetStringUTF16(
                  IDS_PAGE_INFO_SECURITY_TAB_DEPRECATED_SIGNATURE_ALGORITHM_MAJOR);
          break;
        case SecurityStateModel::NO_DEPRECATED_SHA1:
          // Nothing to do.
          break;
      }
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    site_identity_details_.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    if (!security_info.scheme_is_cryptographic || !security_info.cert_id)
      site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
    else
      site_identity_status_ = SITE_IDENTITY_STATUS_ERROR;

    const base::string16 bullet = UTF8ToUTF16("\n • ");
    std::vector<ssl_errors::ErrorInfo> errors;
    ssl_errors::ErrorInfo::GetErrorsForCertStatus(
        cert, security_info.cert_status, url, &errors);
    for (size_t i = 0; i < errors.size(); ++i) {
      site_identity_details_ += bullet;
      site_identity_details_ += errors[i].short_description();
    }

    if (security_info.cert_status & net::CERT_STATUS_NON_UNIQUE_NAME) {
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

  base::string16 subject_name(GetSimpleSiteName(url, profile_));
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
  }

  if (!security_info.cert_id || !security_info.scheme_is_cryptographic) {
    // Page is still loading (so SSL status is not yet available) or
    // loaded over HTTP or loaded over HTTPS with no cert.
    site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;

    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (security_info.security_bits < 0) {
    // Security strength is unknown.  Say nothing.
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
  } else if (security_info.security_bits == 0) {
    DCHECK_NE(security_info.security_level, SecurityStateModel::NONE);
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else {
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED;

    if (net::SSLConnectionStatusToVersion(security_info.connection_status) >=
            net::SSL_CONNECTION_VERSION_TLS1_2 &&
        net::IsSecureTLSCipherSuite(net::SSLConnectionStatusToCipherSuite(
            security_info.connection_status))) {
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
          subject_name));
    } else {
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
          subject_name));
    }

    if (security_info.mixed_content_status !=
        SecurityStateModel::NO_MIXED_CONTENT) {
      bool ran_insecure_content =
          (security_info.mixed_content_status ==
               SecurityStateModel::RAN_MIXED_CONTENT ||
           security_info.mixed_content_status ==
               SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT);
      site_connection_status_ = ran_insecure_content
                                    ? SITE_CONNECTION_STATUS_MIXED_SCRIPT
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
      net::SSLConnectionStatusToCipherSuite(security_info.connection_status);
  if (security_info.security_bits > 0 && cipher_suite) {
    int ssl_version =
        net::SSLConnectionStatusToVersion(security_info.connection_status);
    const char* ssl_version_str;
    net::SSLVersionToString(&ssl_version_str, ssl_version);
    site_connection_details_ += ASCIIToUTF16("\n\n");
    site_connection_details_ += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_SSL_VERSION,
        ASCIIToUTF16(ssl_version_str));

    bool no_renegotiation =
        (security_info.connection_status &
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

    if (ssl_version == net::SSL_CONNECTION_VERSION_SSL3 &&
        site_connection_status_ < SITE_CONNECTION_STATUS_MIXED_CONTENT) {
      site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
    }

    const bool did_fallback = (security_info.connection_status &
                               net::SSL_CONNECTION_VERSION_FALLBACK) != 0;
    if (did_fallback) {
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
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile_);
  DCHECK(delegate);
  // Only show an SSL decision revoke button if the user has chosen to bypass
  // SSL host errors for this host in the past.
  show_ssl_decision_revoke_button_ = delegate->HasAllowException(url.host());

  // By default select the Permissions Tab that displays all the site
  // permissions. In case of a connection error or an issue with the certificate
  // presented by the website, select the Connection Tab to draw the user's
  // attention to the issue. If the site does not provide a certificate because
  // it was loaded over an unencrypted connection, don't select the Connection
  // Tab.
  WebsiteSettingsUI::TabId tab_id = WebsiteSettingsUI::TAB_ID_PERMISSIONS;
  if (site_connection_status_ == SITE_CONNECTION_STATUS_ENCRYPTED_ERROR ||
      site_connection_status_ == SITE_CONNECTION_STATUS_MIXED_CONTENT ||
      site_connection_status_ == SITE_CONNECTION_STATUS_MIXED_SCRIPT ||
      site_identity_status_ == SITE_IDENTITY_STATUS_ERROR ||
      site_identity_status_ == SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN ||
      site_identity_status_ == SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT ||
      site_identity_status_ ==
          SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM_MINOR ||
      site_identity_status_ ==
          SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM_MAJOR) {
    tab_id = WebsiteSettingsUI::TAB_ID_CONNECTION;
    RecordWebsiteSettingsAction(
      WEBSITE_SETTINGS_CONNECTION_TAB_SHOWN_IMMEDIATELY);
  }

  ui_->SetSelectedTab(tab_id);
}

void WebsiteSettings::PresentSitePermissions() {
  PermissionInfoList permission_info_list;

  WebsiteSettingsUI::PermissionInfo permission_info;
  for (size_t i = 0; i < arraysize(kPermissionType); ++i) {
    permission_info.type = kPermissionType[i];

    content_settings::SettingInfo info;
    scoped_ptr<base::Value> value =
        content_settings_->GetWebsiteSetting(
            site_url_, site_url_, permission_info.type, std::string(), &info);
    DCHECK(value.get());
    if (value->GetType() == base::Value::TYPE_INTEGER) {
      permission_info.setting =
          content_settings::ValueToContentSetting(value.get());
    } else {
      NOTREACHED();
    }

    permission_info.source = info.source;

    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      permission_info.default_setting = permission_info.setting;
      permission_info.setting = CONTENT_SETTING_DEFAULT;
    } else {
      permission_info.default_setting =
          content_settings_->GetDefaultContentSetting(permission_info.type,
                                                      NULL);
    }

    if (permission_info.setting != CONTENT_SETTING_DEFAULT &&
        permission_info.setting != permission_info.default_setting) {
      permission_info_list.push_back(permission_info);
    }
  }

  ui_->SetPermissionInfo(permission_info_list);
}

void WebsiteSettings::PresentSiteData() {
  CookieInfoList cookie_info_list;
  const LocalSharedObjectsCounter& allowed_objects =
      tab_specific_content_settings()->allowed_local_shared_objects();
  const LocalSharedObjectsCounter& blocked_objects =
      tab_specific_content_settings()->blocked_local_shared_objects();

  // Add first party cookie and site data counts.
  WebsiteSettingsUI::CookieInfo cookie_info;
  cookie_info.cookie_source =
      l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_FIRST_PARTY_SITE_DATA);
  cookie_info.allowed = allowed_objects.GetObjectCountForDomain(site_url_);
  cookie_info.blocked = blocked_objects.GetObjectCountForDomain(site_url_);
  cookie_info.is_first_party = true;
  cookie_info_list.push_back(cookie_info);

  // Add third party cookie counts.
  cookie_info.cookie_source = l10n_util::GetStringUTF8(
     IDS_WEBSITE_SETTINGS_THIRD_PARTY_SITE_DATA);
  cookie_info.allowed = allowed_objects.GetObjectCount() - cookie_info.allowed;
  cookie_info.blocked = blocked_objects.GetObjectCount() - cookie_info.blocked;
  cookie_info.is_first_party = false;
  cookie_info_list.push_back(cookie_info);

  ui_->SetCookieInfo(cookie_info_list);
}

void WebsiteSettings::PresentSiteIdentity() {
  // After initialization the status about the site's connection and its
  // identity must be available.
  DCHECK_NE(site_identity_status_, SITE_IDENTITY_STATUS_UNKNOWN);
  DCHECK_NE(site_connection_status_, SITE_CONNECTION_STATUS_UNKNOWN);
  WebsiteSettingsUI::IdentityInfo info;
  if (site_identity_status_ == SITE_IDENTITY_STATUS_EV_CERT)
    info.site_identity = UTF16ToUTF8(organization_name());
  else
    info.site_identity = UTF16ToUTF8(GetSimpleSiteName(site_url_, profile_));

  info.connection_status = site_connection_status_;
  info.connection_status_description =
      UTF16ToUTF8(site_connection_details_);
  info.identity_status = site_identity_status_;
  info.identity_status_description =
      UTF16ToUTF8(site_identity_details_);
  info.cert_id = cert_id_;
  info.show_ssl_decision_revoke_button = show_ssl_decision_revoke_button_;
  ui_->SetIdentityInfo(info);
}
