// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
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
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
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
#include "content/public/browser/permission_type.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "grit/components_chromium_strings.h"
#include "grit/components_google_chrome_strings.h"
#include "grit/components_strings.h"
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
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/website_settings/website_settings_infobar_delegate.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
using content::BrowserThread;
using security_state::SecurityStateModel;

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
    CONTENT_SETTINGS_TYPE_KEYGEN,
};

// Determines whether to show permission |type| in the Website Settings UI. Only
// applies to permissions listed in |kPermissionType|.
bool ShouldShowPermission(ContentSettingsType type) {
  // TODO(mgiuca): When simplified-fullscreen-ui is enabled on all platforms,
  // remove these from kPermissionType, rather than having this check
  // (http://crbug.com/577396).
#if !defined(OS_ANDROID)
  // Fullscreen and mouselock settings are not shown in simplified fullscreen
  // mode (always allow).
  if (type == CONTENT_SETTINGS_TYPE_FULLSCREEN ||
      type == CONTENT_SETTINGS_TYPE_MOUSELOCK) {
    return !ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled();
  }
#endif

  return true;
}

// Returns true if any of the given statuses match |status|.
bool CertificateTransparencyStatusMatchAny(
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

  // Any valid SCT.
  if (CertificateTransparencyStatusMatchAny(sct_verify_statuses,
                                            net::ct::SCT_STATUS_OK))
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_VERIFIED
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_VERIFIED);

  // Any invalid SCT.
  if (CertificateTransparencyStatusMatchAny(sct_verify_statuses,
                                            net::ct::SCT_STATUS_INVALID))
    return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_INVALID
                  : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_INVALID);

  // All SCTs are from unknown logs.
  return (is_ev ? IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_CT_UNVERIFIED
                : IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_CT_UNVERIFIED);
}

// This function will return SITE_IDENTITY_STATUS_CERT or
// SITE_IDENTITY_STATUS_EV_CERT depending on |is_ev| unless all SCTs
// failed verification, in which case it will return
// SITE_IDENTITY_STATUS_ERROR.
WebsiteSettings::SiteIdentityStatus GetSiteIdentityStatusByCTInfo(
    const std::vector<net::ct::SCTVerifyStatus>& sct_verify_statuses,
    bool is_ev) {
  if (sct_verify_statuses.empty() ||
      CertificateTransparencyStatusMatchAny(sct_verify_statuses,
                                            net::ct::SCT_STATUS_OK))
    return is_ev ? WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT
                 : WebsiteSettings::SITE_IDENTITY_STATUS_CERT;

  return WebsiteSettings::SITE_IDENTITY_STATUS_CT_ERROR;
}

base::string16 GetSimpleSiteName(const GURL& url, Profile* profile) {
  std::string languages;
  if (profile)
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
  return url_formatter::FormatUrlForSecurityDisplayOmitScheme(url, languages);
}

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

// The list of chooser types that need to display entries in the Website
// Settings UI. THE ORDER OF THESE ITEMS IS IMPORTANT. To propose changing it,
// email security-dev@chromium.org.
WebsiteSettings::ChooserUIInfo kChooserUIInfo[] = {
    {&GetUsbChooserContext, IDR_BLOCKED_USB, IDR_ALLOWED_USB,
     IDS_WEBSITE_SETTINGS_USB_DEVICE_LABEL,
     IDS_WEBSITE_SETTINGS_DELETE_USB_DEVICE, "name"},
};

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
    content::PermissionType permission_type;
    if (PermissionUtil::GetPermissionType(type, &permission_type)) {
      PermissionUmaUtil::PermissionRevoked(permission_type,
                                           this->site_url_);
    }
  }

  // This is technically redundant given the histogram above, but putting the
  // total count of permission changes in another histogram makes it easier to
  // compare it against other kinds of actions in WebsiteSettings[PopupView].
  RecordWebsiteSettingsAction(WEBSITE_SETTINGS_CHANGED_PERMISSION);

  content_settings_->SetNarrowestContentSetting(site_url_, site_url_, type,
                                                setting);

  show_info_bar_ = true;

  // Refresh the UI to reflect the new setting.
  PresentSitePermissions();
}

void WebsiteSettings::OnSiteChosenObjectDeleted(
    const ChooserUIInfo& ui_info,
    const base::DictionaryValue& object) {
  // TODO(reillyg): Create metrics for revocations. crbug.com/556845
  ChooserContextBase* context = ui_info.get_context(profile_);
  const GURL origin = site_url_.GetOrigin();
  context->RevokeObjectPermission(origin, origin, object);

  show_info_bar_ = true;

  // Refresh the UI to reflect the changed settings.
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
#if BUILDFLAG(ANDROID_JAVA_UI)
  isChromeUINativeScheme = url.SchemeIs(chrome::kChromeUINativeScheme);
#endif

  if (url.SchemeIs(url::kAboutScheme)) {
    // All about: URLs except about:blank are redirected.
    DCHECK_EQ(url::kAboutBlankURL, url.spec());
    site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
    site_identity_details_ =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY);
    site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;
    site_connection_details_ = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        UTF8ToUTF16(url.spec()));
    return;
  }

  if (url.SchemeIs(content::kChromeUIScheme) || isChromeUINativeScheme) {
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

    if (security_info.is_secure_protocol_and_ciphersuite) {
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

  uint16_t cipher_suite =
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
      site_identity_status_ == SITE_IDENTITY_STATUS_CT_ERROR ||
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
  ChosenObjectInfoList chosen_object_info_list;

  WebsiteSettingsUI::PermissionInfo permission_info;
  for (size_t i = 0; i < arraysize(kPermissionType); ++i) {
    permission_info.type = kPermissionType[i];

    if (!ShouldShowPermission(permission_info.type))
      continue;

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
    permission_info.is_incognito = profile_->IsOffTheRecord();

    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      permission_info.default_setting = permission_info.setting;
      permission_info.setting = CONTENT_SETTING_DEFAULT;
    } else {
      permission_info.default_setting =
          content_settings_->GetDefaultContentSetting(permission_info.type,
                                                      NULL);
    }

    if ((permission_info.setting != CONTENT_SETTING_DEFAULT &&
         permission_info.setting != permission_info.default_setting) ||
        (permission_info.type == CONTENT_SETTINGS_TYPE_KEYGEN &&
         tab_specific_content_settings()->IsContentBlocked(
             permission_info.type))) {
      permission_info_list.push_back(permission_info);
    }
  }

  for (const ChooserUIInfo& ui_info : kChooserUIInfo) {
    ChooserContextBase* context = ui_info.get_context(profile_);
    const GURL origin = site_url_.GetOrigin();
    auto chosen_objects = context->GetGrantedObjects(origin, origin);
    for (scoped_ptr<base::DictionaryValue>& object : chosen_objects) {
      chosen_object_info_list.push_back(
          new WebsiteSettingsUI::ChosenObjectInfo(ui_info, std::move(object)));
    }
  }

  ui_->SetPermissionInfo(permission_info_list, chosen_object_info_list);
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
