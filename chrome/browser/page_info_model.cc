// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info_model.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/page_info_model_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "content/browser/cert_store.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_cipher_suite_names.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

PageInfoModel::PageInfoModel(Profile* profile,
                             const GURL& url,
                             const NavigationEntry::SSLStatus& ssl,
                             bool show_history,
                             PageInfoModelObserver* observer)
    : observer_(observer) {
  Init();

  if (url.SchemeIs(chrome::kChromeUIScheme)) {
    sections_.push_back(
        SectionInfo(ICON_STATE_INTERNAL_PAGE,
                    string16(),
                    l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE),
                    SECTION_INFO_INTERNAL_PAGE));
    return;
  }

  SectionStateIcon icon_id = ICON_STATE_OK;
  string16 headline;
  string16 description;
  scoped_refptr<net::X509Certificate> cert;

  // Identity section.
  string16 subject_name(UTF8ToUTF16(url.host()));
  bool empty_subject_name = false;
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
    empty_subject_name = true;
  }

  if (ssl.cert_id() &&
      CertStore::GetInstance()->RetrieveCert(ssl.cert_id(), &cert) &&
      (!net::IsCertStatusError(ssl.cert_status()) ||
       net::IsCertStatusMinorError(ssl.cert_status()))) {
    // There are no major errors. Check for minor errors.
    if (net::IsCertStatusMinorError(ssl.cert_status())) {
      string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));

      description += ASCIIToUTF16("\n\n");
      if (ssl.cert_status() & net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION) {
        description += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNABLE_TO_CHECK_REVOCATION);
      } else if (ssl.cert_status() & net::CERT_STATUS_NO_REVOCATION_MECHANISM) {
        description += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_NO_REVOCATION_MECHANISM);
      } else {
        NOTREACHED() << "Need to specify string for this warning";
      }
      icon_id = ICON_STATE_WARNING_MINOR;
    } else if (ssl.cert_status() & net::CERT_STATUS_IS_EV) {
      // EV HTTPS page.
      DCHECK(!cert->subject().organization_names.empty());
      headline =
          l10n_util::GetStringFUTF16(IDS_PAGE_INFO_EV_IDENTITY_TITLE,
              UTF8ToUTF16(cert->subject().organization_names[0]),
              UTF8ToUTF16(url.host()));
      // An EV Cert is required to have a city (localityName) and country but
      // state is "if any".
      DCHECK(!cert->subject().locality_name.empty());
      DCHECK(!cert->subject().country_name.empty());
      string16 locality;
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
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV,
          UTF8ToUTF16(cert->subject().organization_names[0]),
          locality,
          UTF8ToUTF16(cert->issuer().GetDisplayName())));
    } else if (ssl.cert_status() & net::CERT_STATUS_IS_DNSSEC) {
      // DNSSEC authenticated page.
      if (empty_subject_name)
        headline.clear();  // Don't display any title.
      else
        headline.assign(subject_name);
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, UTF8ToUTF16("DNSSEC")));
    } else {
      // Non-EV OK HTTPS page.
      if (empty_subject_name)
        headline.clear();  // Don't display any title.
      else
        headline.assign(subject_name);
      string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    description.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    icon_id = ssl.security_style() == content::SECURITY_STYLE_UNAUTHENTICATED ?
        ICON_STATE_WARNING_MAJOR : ICON_STATE_ERROR;

    const string16 bullet = UTF8ToUTF16("\n • ");
    std::vector<SSLErrorInfo> errors;
    SSLErrorInfo::GetErrorsForCertStatus(ssl.cert_id(), ssl.cert_status(),
                                         url, &errors);
    for (size_t i = 0; i < errors.size(); ++i) {
      description += bullet;
      description += errors[i].short_description();
    }

    if (ssl.cert_status() & net::CERT_STATUS_NON_UNIQUE_NAME) {
      description += ASCIIToUTF16("\n\n");
      description += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NON_UNIQUE_NAME);
    }
  }
  sections_.push_back(SectionInfo(
      icon_id,
      headline,
      description,
      SECTION_INFO_IDENTITY));

  // Connection section.
  // We consider anything less than 80 bits encryption to be weak encryption.
  // TODO(wtc): Bug 1198735: report mixed/unsafe content for unencrypted and
  // weakly encrypted connections.
  icon_id = ICON_STATE_OK;
  headline.clear();
  description.clear();
  if (!ssl.cert_id()) {
    // Not HTTPS.
    DCHECK_EQ(ssl.security_style(), content::SECURITY_STYLE_UNAUTHENTICATED);
    icon_id = ssl.security_style() == content::SECURITY_STYLE_UNAUTHENTICATED ?
        ICON_STATE_WARNING_MAJOR : ICON_STATE_ERROR;
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (ssl.security_bits() < 0) {
    // Security strength is unknown.  Say nothing.
    icon_id = ICON_STATE_ERROR;
  } else if (ssl.security_bits() == 0) {
    DCHECK_NE(ssl.security_style(), content::SECURITY_STYLE_UNAUTHENTICATED);
    icon_id = ICON_STATE_ERROR;
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (ssl.security_bits() < 80) {
    icon_id = ICON_STATE_ERROR;
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
        subject_name));
  } else {
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
        subject_name,
        base::IntToString16(ssl.security_bits())));
    if (ssl.displayed_insecure_content() || ssl.ran_insecure_content()) {
      icon_id = ssl.ran_insecure_content() ?
          ICON_STATE_ERROR : ICON_STATE_WARNING_MINOR;
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
          description,
          l10n_util::GetStringUTF16(ssl.ran_insecure_content() ?
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_ERROR :
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING)));
    }
  }

  uint16 cipher_suite =
      net::SSLConnectionStatusToCipherSuite(ssl.connection_status());
  if (ssl.security_bits() > 0 && cipher_suite) {
    int ssl_version =
        net::SSLConnectionStatusToVersion(ssl.connection_status());
    const char* ssl_version_str;
    net::SSLVersionToString(&ssl_version_str, ssl_version);
    description += ASCIIToUTF16("\n\n");
    description += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_SSL_VERSION,
        ASCIIToUTF16(ssl_version_str));

    bool did_fallback = (ssl.connection_status() &
                         net::SSL_CONNECTION_SSL3_FALLBACK) != 0;
    bool no_renegotiation =
        (ssl.connection_status() &
        net::SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION) != 0;
    const char *key_exchange, *cipher, *mac;
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, cipher_suite);

    description += ASCIIToUTF16("\n\n");
    description += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS,
        ASCIIToUTF16(cipher), ASCIIToUTF16(mac), ASCIIToUTF16(key_exchange));

    description += ASCIIToUTF16("\n\n");
    uint8 compression_id =
        net::SSLConnectionStatusToCompression(ssl.connection_status());
    if (compression_id) {
      const char* compression;
      net::SSLCompressionToString(&compression, compression_id);
      description += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_COMPRESSION_DETAILS,
          ASCIIToUTF16(compression));
    } else {
      description += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NO_COMPRESSION);
    }

    if (did_fallback) {
      // For now, only SSLv3 fallback will trigger a warning icon.
      if (icon_id < ICON_STATE_WARNING_MINOR)
        icon_id = ICON_STATE_WARNING_MINOR;
      description += ASCIIToUTF16("\n\n");
      description += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_FALLBACK_MESSAGE);
    }
    if (no_renegotiation) {
      description += ASCIIToUTF16("\n\n");
      description += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_RENEGOTIATION_MESSAGE);
    }
  }

  if (!description.empty()) {
    sections_.push_back(SectionInfo(
        icon_id,
        headline,
        description,
        SECTION_INFO_CONNECTION));
  }

  // Request the number of visits.
  HistoryService* history = profile->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (show_history && history) {
    history->GetVisibleVisitCountToHost(
        url,
        &request_consumer_,
        base::Bind(&PageInfoModel::OnGotVisitCountToHost,
                   base::Unretained(this)));
  }
}

PageInfoModel::~PageInfoModel() {}

int PageInfoModel::GetSectionCount() {
  return sections_.size();
}

PageInfoModel::SectionInfo PageInfoModel::GetSectionInfo(int index) {
  DCHECK(index < static_cast<int>(sections_.size()));
  return sections_[index];
}

gfx::Image* PageInfoModel::GetIconImage(SectionStateIcon icon_id) {
  if (icon_id == ICON_NONE)
    return NULL;
  // The bubble uses new, various icons.
  return icons_[icon_id];
}

void PageInfoModel::OnGotVisitCountToHost(HistoryService::Handle handle,
                                          bool found_visits,
                                          int count,
                                          base::Time first_visit) {
  if (!found_visits) {
    // This indicates an error, such as the page wasn't http/https; do nothing.
    return;
  }

  bool visited_before_today = false;
  if (count) {
    base::Time today = base::Time::Now().LocalMidnight();
    base::Time first_visit_midnight = first_visit.LocalMidnight();
    visited_before_today = (first_visit_midnight < today);
  }

  string16 headline = l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_INFO_TITLE);

  if (!visited_before_today) {
    sections_.push_back(SectionInfo(
        ICON_STATE_WARNING_MAJOR,
        headline,
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_FIRST_VISITED_TODAY),
        SECTION_INFO_FIRST_VISIT));
  } else {
    sections_.push_back(SectionInfo(
        ICON_STATE_INFO,
        headline,
        l10n_util::GetStringFUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_VISITED_BEFORE_TODAY,
            base::TimeFormatShortDate(first_visit)),
        SECTION_INFO_FIRST_VISIT));
  }
  observer_->OnPageInfoModelChanged();
}

PageInfoModel::PageInfoModel() : observer_(NULL) {
  Init();
}

void PageInfoModel::Init() {
  // Loads the icons into the vector. The order must match the SectionStateIcon
  // enum.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  icons_.push_back(&rb.GetNativeImageNamed(IDR_PAGEINFO_GOOD));
  icons_.push_back(&rb.GetNativeImageNamed(IDR_PAGEINFO_WARNING_MINOR));
  icons_.push_back(&rb.GetNativeImageNamed(IDR_PAGEINFO_WARNING_MAJOR));
  icons_.push_back(&rb.GetNativeImageNamed(IDR_PAGEINFO_BAD));
  icons_.push_back(&rb.GetNativeImageNamed(IDR_PAGEINFO_INFO));
  icons_.push_back(&rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_26));
}
