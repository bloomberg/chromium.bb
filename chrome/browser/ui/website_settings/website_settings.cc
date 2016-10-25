// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings.h"

#include <openssl/ssl.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
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
#include "chrome/browser/content_settings/local_shared_objects_container.h"
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
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/rappor/rappor_utils.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
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
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
#if !defined(OS_ANDROID)
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTINGS_TYPE_IMAGES,
#endif
    CONTENT_SETTINGS_TYPE_POPUPS,
    CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
    CONTENT_SETTINGS_TYPE_KEYGEN,
    CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
#if !defined(OS_ANDROID)
    CONTENT_SETTINGS_TYPE_MOUSELOCK,
#endif
    CONTENT_SETTINGS_TYPE_FULLSCREEN,
    CONTENT_SETTINGS_TYPE_AUTOPLAY,
    CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
};

// Determines whether to show permission |type| in the Website Settings UI. Only
// applies to permissions listed in |kPermissionType|.
bool ShouldShowPermission(ContentSettingsType type) {
  // TODO(mgiuca): When simplified-fullscreen-ui is enabled permanently on
  // Android, remove these from kPermissionType, rather than having this check
  // (http://crbug.com/577396).
#if !defined(OS_ANDROID)
  // Fullscreen and mouselock settings are no longer shown (always allow).
  // Autoplay is Android-only at the moment.
  if (type == CONTENT_SETTINGS_TYPE_AUTOPLAY ||
      type == CONTENT_SETTINGS_TYPE_FULLSCREEN ||
      type == CONTENT_SETTINGS_TYPE_MOUSELOCK) {
    return false;
  }
#endif

  return true;
}

void CheckContentStatus(SecurityStateModel::ContentStatus content_status,
                        bool* displayed,
                        bool* ran) {
  switch (content_status) {
    case SecurityStateModel::CONTENT_STATUS_DISPLAYED:
      *displayed = true;
      break;
    case SecurityStateModel::CONTENT_STATUS_RAN:
      *ran = true;
      break;
    case SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN:
      *displayed = true;
      *ran = true;
      break;
    case SecurityStateModel::CONTENT_STATUS_UNKNOWN:
    case SecurityStateModel::CONTENT_STATUS_NONE:
      break;
  }
}

void CheckForInsecureContent(
    const SecurityStateModel::SecurityInfo& security_info,
    bool* displayed,
    bool* ran) {
  CheckContentStatus(security_info.mixed_content_status, displayed, ran);
  // Only consider subresources with certificate errors if the main
  // resource was loaded over HTTPS without major certificate errors. If
  // the main resource had a certificate error, then it would not be
  // that useful (and would potentially be confusing) to warn about
  // subesources that had certificate errors too.
  if (net::IsCertStatusError(security_info.cert_status) &&
      !net::IsCertStatusMinorError(security_info.cert_status)) {
    return;
  }
  CheckContentStatus(security_info.content_with_cert_errors_status, displayed,
                     ran);
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
  if (CertificateTransparencyStatusMatchAny(
          sct_verify_statuses, net::ct::SCT_STATUS_INVALID_TIMESTAMP) ||
      CertificateTransparencyStatusMatchAny(
          sct_verify_statuses, net::ct::SCT_STATUS_INVALID_SIGNATURE))
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

base::string16 GetSimpleSiteName(const GURL& url) {
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

// The list of chooser types that need to display entries in the Website
// Settings UI. THE ORDER OF THESE ITEMS IS IMPORTANT. To propose changing it,
// email security-dev@chromium.org.
WebsiteSettings::ChooserUIInfo kChooserUIInfo[] = {
    {CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, &GetUsbChooserContext,
     IDR_BLOCKED_USB, IDR_ALLOWED_USB, IDS_WEBSITE_SETTINGS_USB_DEVICE_LABEL,
     IDS_WEBSITE_SETTINGS_DELETE_USB_DEVICE, "name"},
};

}  // namespace

WebsiteSettings::WebsiteSettings(
    WebsiteSettingsUI* ui,
    Profile* profile,
    TabSpecificContentSettings* tab_specific_content_settings,
    content::WebContents* web_contents,
    const GURL& url,
    const SecurityStateModel::SecurityInfo& security_info)
    : TabSpecificContentSettings::SiteDataObserver(
          tab_specific_content_settings),
      content::WebContentsObserver(web_contents),
      ui_(ui),
      show_info_bar_(false),
      site_url_(url),
      site_identity_status_(SITE_IDENTITY_STATUS_UNKNOWN),
      site_connection_status_(SITE_CONNECTION_STATUS_UNKNOWN),
      show_ssl_decision_revoke_button_(false),
      content_settings_(HostContentSettingsMapFactory::GetForProfile(profile)),
      chrome_ssl_host_state_delegate_(
          ChromeSSLHostStateDelegateFactory::GetForProfile(profile)),
      did_revoke_user_ssl_decisions_(false),
      profile_(profile),
      security_level_(security_state::SecurityStateModel::NONE) {
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

  std::string histogram_name;

  if (site_url_.SchemeIsCryptographic()) {
    if (security_level_ == security_state::SecurityStateModel::SECURE ||
        security_level_ == security_state::SecurityStateModel::EV_SECURE) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.Valid",
                                action, WEBSITE_SETTINGS_COUNT);
    } else if (security_level_ == security_state::SecurityStateModel::NONE) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.Downgraded",
                                action, WEBSITE_SETTINGS_COUNT);
    } else if (security_level_ ==
               security_state::SecurityStateModel::DANGEROUS) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.Dangerous",
                                action, WEBSITE_SETTINGS_COUNT);
    }
    return;
  }

  if (security_level_ ==
      security_state::SecurityStateModel::HTTP_SHOW_WARNING) {
    UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpUrl.Warning",
                              action, WEBSITE_SETTINGS_COUNT);
  } else if (security_level_ == security_state::SecurityStateModel::DANGEROUS) {
    UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpUrl.Dangerous",
                              action, WEBSITE_SETTINGS_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpUrl.Neutral",
                              action, WEBSITE_SETTINGS_COUNT);
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

    if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.Plugins.AddedAllowException", site_url_);
    }
  } else if (setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebsiteSettings.OriginInfo.PermissionChanged.Blocked", histogram_value,
        num_values);
  }

  // This is technically redundant given the histogram above, but putting the
  // total count of permission changes in another histogram makes it easier to
  // compare it against other kinds of actions in WebsiteSettings[PopupView].
  RecordWebsiteSettingsAction(WEBSITE_SETTINGS_CHANGED_PERMISSION);

  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      this->profile_, this->site_url_, this->site_url_, type,
      PermissionSourceUI::OIB);

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
  if (show_info_bar_ && web_contents() && !web_contents()->IsBeingDestroyed()) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents());
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
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // On desktop, internal URLs aren't handled by this class. Instead, a
  // custom and simpler popup is shown.
  DCHECK(!url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) &&
         !url.SchemeIs(content::kViewSourceScheme) &&
         !url.SchemeIs(content_settings::kExtensionScheme));
#endif

  bool isChromeUINativeScheme = false;
#if BUILDFLAG(ANDROID_JAVA_UI)
  isChromeUINativeScheme = url.SchemeIs(chrome::kChromeUINativeScheme);
#endif

  security_level_ = security_info.security_level;

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
  certificate_ = security_info.certificate;

  // HTTPS with no or minor errors.
  if (certificate_ &&
      (!net::IsCertStatusError(security_info.cert_status) ||
       net::IsCertStatusMinorError(security_info.cert_status))) {
    // There are no major errors. Check for minor errors.
    if (security_info.security_level ==
        SecurityStateModel::SECURE_WITH_POLICY_INSTALLED_CERT) {
      site_identity_status_ = SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      site_identity_details_ = l10n_util::GetStringFUTF16(
          IDS_CERT_POLICY_PROVIDED_CERT_MESSAGE, UTF8ToUTF16(url.host()));
    } else if (net::IsCertStatusMinorError(security_info.cert_status)) {
      site_identity_status_ = SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN;
      base::string16 issuer_name(
          UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
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
        DCHECK(!certificate_->subject().organization_names.empty());
        organization_name_ =
            UTF8ToUTF16(certificate_->subject().organization_names[0]);
        // An EV Cert is required to have a city (localityName) and country but
        // state is "if any".
        DCHECK(!certificate_->subject().locality_name.empty());
        DCHECK(!certificate_->subject().country_name.empty());
        base::string16 locality;
        if (!certificate_->subject().state_or_province_name.empty()) {
          locality = l10n_util::GetStringFUTF16(
              IDS_PAGEINFO_ADDRESS,
              UTF8ToUTF16(certificate_->subject().locality_name),
              UTF8ToUTF16(certificate_->subject().state_or_province_name),
              UTF8ToUTF16(certificate_->subject().country_name));
        } else {
          locality = l10n_util::GetStringFUTF16(
              IDS_PAGEINFO_PARTIAL_ADDRESS,
              UTF8ToUTF16(certificate_->subject().locality_name),
              UTF8ToUTF16(certificate_->subject().country_name));
        }
        DCHECK(!certificate_->subject().organization_names.empty());
        site_identity_details_.assign(l10n_util::GetStringFUTF16(
            GetSiteIdentityDetailsMessageByCTInfo(
                security_info.sct_verify_statuses, true /* is EV */),
            UTF8ToUTF16(certificate_->subject().organization_names[0]),
            locality,
            UTF8ToUTF16(certificate_->issuer().GetDisplayName())));
      } else {
        // Non-EV OK HTTPS page.
        site_identity_status_ = GetSiteIdentityStatusByCTInfo(
            security_info.sct_verify_statuses, false);
        base::string16 issuer_name(
            UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
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
        case SecurityStateModel::UNKNOWN_SHA1:
          // UNKNOWN_SHA1 should only appear when certificate info has not been
          // initialized, in which case this if-statement should not be running
          // because there is no other cert info.
          NOTREACHED();
      }
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    site_identity_details_.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    if (!security_info.scheme_is_cryptographic || !security_info.certificate)
      site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
    else
      site_identity_status_ = SITE_IDENTITY_STATUS_ERROR;

    const base::string16 bullet = UTF8ToUTF16("\n • ");
    std::vector<ssl_errors::ErrorInfo> errors;
    ssl_errors::ErrorInfo::GetErrorsForCertStatus(
        certificate_, security_info.cert_status, url, &errors);
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

  base::string16 subject_name(GetSimpleSiteName(url));
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
  }

  if (!security_info.certificate || !security_info.scheme_is_cryptographic) {
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

    if (security_info.obsolete_ssl_status == net::OBSOLETE_SSL_NONE) {
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
          subject_name));
    } else {
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
          subject_name));
    }

    bool ran_insecure_content = false;
    bool displayed_insecure_content = false;
    CheckForInsecureContent(security_info, &displayed_insecure_content,
                            &ran_insecure_content);
    if (ran_insecure_content || displayed_insecure_content) {
      site_connection_status_ =
          ran_insecure_content
              ? SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE
              : SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE;
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
          site_connection_details_,
          l10n_util::GetStringUTF16(
              ran_insecure_content
                  ? IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_ERROR
                  : IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING)));
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
    bool is_aead, is_tls13;
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);

    site_connection_details_ += ASCIIToUTF16("\n\n");
    if (is_aead) {
      if (is_tls13) {
        // For TLS 1.3 ciphers, report the group (historically, curve) as the
        // key exchange.
        key_exchange = SSL_get_curve_name(security_info.key_exchange_group);
        if (!key_exchange) {
          NOTREACHED();
          key_exchange = "";
        }
      }
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS_AEAD,
          ASCIIToUTF16(cipher), ASCIIToUTF16(key_exchange));
    } else {
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS,
          ASCIIToUTF16(cipher), ASCIIToUTF16(mac), ASCIIToUTF16(key_exchange));
    }

    if (ssl_version == net::SSL_CONNECTION_VERSION_SSL3 &&
        site_connection_status_ <
            SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE) {
      site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
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
      site_connection_status_ ==
          SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE ||
      site_connection_status_ ==
          SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE ||
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
    std::unique_ptr<base::Value> value = content_settings_->GetWebsiteSetting(
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

    if (permission_info.type == CONTENT_SETTINGS_TYPE_KEYGEN &&
        (permission_info.setting == CONTENT_SETTING_DEFAULT ||
         permission_info.setting == permission_info.default_setting) &&
        !tab_specific_content_settings()->IsContentBlocked(
            permission_info.type)) {
      continue;
    }
    permission_info_list.push_back(permission_info);
  }

  for (const ChooserUIInfo& ui_info : kChooserUIInfo) {
    ChooserContextBase* context = ui_info.get_context(profile_);
    const GURL origin = site_url_.GetOrigin();
    auto chosen_objects = context->GetGrantedObjects(origin, origin);
    for (std::unique_ptr<base::DictionaryValue>& object : chosen_objects) {
      chosen_object_info_list.push_back(
          base::MakeUnique<WebsiteSettingsUI::ChosenObjectInfo>(
              ui_info, std::move(object)));
    }
  }

  ui_->SetPermissionInfo(permission_info_list,
                         std::move(chosen_object_info_list));
}

void WebsiteSettings::PresentSiteData() {
  CookieInfoList cookie_info_list;
  const LocalSharedObjectsContainer& allowed_objects =
      tab_specific_content_settings()->allowed_local_shared_objects();
  const LocalSharedObjectsContainer& blocked_objects =
      tab_specific_content_settings()->blocked_local_shared_objects();

  // Add first party cookie and site data counts.
  WebsiteSettingsUI::CookieInfo cookie_info;
  cookie_info.allowed = allowed_objects.GetObjectCountForDomain(site_url_);
  cookie_info.blocked = blocked_objects.GetObjectCountForDomain(site_url_);
  cookie_info.is_first_party = true;
  cookie_info_list.push_back(cookie_info);

  // Add third party cookie counts.
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
    info.site_identity = UTF16ToUTF8(GetSimpleSiteName(site_url_));

  info.connection_status = site_connection_status_;
  info.connection_status_description =
      UTF16ToUTF8(site_connection_details_);
  info.identity_status = site_identity_status_;
  info.identity_status_description =
      UTF16ToUTF8(site_identity_details_);
  info.certificate = certificate_;
  info.show_ssl_decision_revoke_button = show_ssl_decision_revoke_button_;
  ui_->SetIdentityInfo(info);
}
