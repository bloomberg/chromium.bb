// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/page_info_model.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/omnibox/page_info_model_observer.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/ssl_status.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// TODO(crbug.com/227827) Merge 178763: PageInfoModel has been removed in
// upstream; check if we should use PageInfoModel.
PageInfoModel::PageInfoModel(ios::ChromeBrowserState* browser_state,
                             const GURL& url,
                             const web::SSLStatus& ssl,
                             PageInfoModelObserver* observer)
    : observer_(observer) {
  if (url.SchemeIs(kChromeUIScheme)) {
    if (url.host() == kChromeUIOfflineHost) {
      sections_.push_back(SectionInfo(
          ICON_STATE_OFFLINE_PAGE,
          l10n_util::GetStringUTF16(IDS_IOS_PAGE_INFO_OFFLINE_TITLE),
          l10n_util::GetStringUTF16(IDS_IOS_PAGE_INFO_OFFLINE_PAGE),
          SECTION_INFO_INTERNAL_PAGE, BUTTON_RELOAD));
    } else {
      sections_.push_back(
          SectionInfo(ICON_STATE_INTERNAL_PAGE, base::string16(),
                      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE),
                      SECTION_INFO_INTERNAL_PAGE, BUTTON_NONE));
    }
    return;
  }

  SectionStateIcon icon_id = ICON_STATE_OK;
  base::string16 headline;
  base::string16 description;

  // Identity section.
  base::string16 subject_name(base::UTF8ToUTF16(url.host()));
  bool empty_subject_name = false;
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
    empty_subject_name = true;
  }

  bool is_cert_present = !!ssl.certificate;
  bool is_major_cert_error = net::IsCertStatusError(ssl.cert_status) &&
                             !net::IsCertStatusMinorError(ssl.cert_status);

  // It is possible to have |SECURITY_STYLE_AUTHENTICATION_BROKEN| and non-error
  // |cert_status| for WKWebView because |security_style| and |cert_status| are
  // calculated using different API, which may lead to different cert
  // verification results.
  if (is_cert_present && !is_major_cert_error &&
      ssl.security_style != web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
    // There are no major errors. Check for minor errors.
    if (net::IsCertStatusMinorError(ssl.cert_status)) {
      base::string16 issuer_name(
          base::UTF8ToUTF16(ssl.certificate->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }
      description.assign(l10n_util::GetStringFUTF16(
          IDS_IOS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));

      description += base::ASCIIToUTF16("\n\n");
      if (ssl.cert_status & net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION) {
        description += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNABLE_TO_CHECK_REVOCATION);
      } else if (ssl.cert_status & net::CERT_STATUS_NO_REVOCATION_MECHANISM) {
        description += l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_NO_REVOCATION_MECHANISM);
      } else {
        NOTREACHED() << "Need to specify string for this warning";
      }
      icon_id = ICON_STATE_INFO;
    } else {
      // OK HTTPS page.
      DCHECK(!(ssl.cert_status & net::CERT_STATUS_IS_EV))
          << "Extended Validation should be disabled";
      if (empty_subject_name)
        headline.clear();  // Don't display any title.
      else
        headline.assign(subject_name);
      base::string16 issuer_name(
          base::UTF8ToUTF16(ssl.certificate->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }
      description.assign(l10n_util::GetStringFUTF16(
          IDS_IOS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));
    }
    if (ssl.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT) {
      icon_id = ICON_STATE_INFO;
      description +=
          base::UTF8ToUTF16("\n\n") +
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_DEPRECATED_SIGNATURE_ALGORITHM);
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    description.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    icon_id = ssl.security_style == web::SECURITY_STYLE_UNAUTHENTICATED
                  ? ICON_STATE_INFO
                  : ICON_STATE_ERROR;

    const base::string16 bullet = base::UTF8ToUTF16("\n • ");
    std::vector<ssl_errors::ErrorInfo> errors;
    ssl_errors::ErrorInfo::GetErrorsForCertStatus(
        ssl.certificate, ssl.cert_status, url, &errors);
    for (size_t i = 0; i < errors.size(); ++i) {
      description += bullet;
      description += errors[i].short_description();
    }

    if (ssl.cert_status & net::CERT_STATUS_NON_UNIQUE_NAME) {
      description += base::ASCIIToUTF16("\n\n");
      description +=
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_NON_UNIQUE_NAME);
    }
  }
  sections_.push_back(SectionInfo(icon_id, headline, description,
                                  SECTION_INFO_IDENTITY,
                                  BUTTON_SHOW_SECURITY_HELP));

  // Connection section.
  icon_id = ICON_STATE_OK;
  headline.clear();
  description.clear();
  if (!ssl.certificate) {
    // Not HTTPS.
    icon_id = ssl.security_style == web::SECURITY_STYLE_UNAUTHENTICATED
                  ? ICON_STATE_INFO
                  : ICON_STATE_ERROR;
    description.assign(
        l10n_util::GetStringUTF16(IDS_PAGEINFO_NOT_SECURE_SUMMARY));
    description += base::ASCIIToUTF16("\n\n");
    description += l10n_util::GetStringUTF16(IDS_PAGEINFO_NOT_SECURE_DETAILS);
  } else if (ssl.security_bits < 0) {
    if (ssl.content_status == web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
      DCHECK(description.empty());
      // For WKWebView security_bits flag is always -1, and description is empty
      // because ciphersuite is unknown. On iOS9 WKWebView blocks active
      // mixed content, so warning should be about page look, not about page
      // behavior.
      icon_id = ICON_NONE;
      description.assign(l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING));
    } else {
      // Security strength is unknown.  Say nothing.
      icon_id = ICON_STATE_ERROR;
    }
  } else if (ssl.security_bits == 0) {
    DCHECK_NE(ssl.security_style, web::SECURITY_STYLE_UNAUTHENTICATED);
    icon_id = ICON_STATE_ERROR;
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else {
    if (net::SSLConnectionStatusToVersion(ssl.connection_status) >=
            net::SSL_CONNECTION_VERSION_TLS1_2 &&
        (net::OBSOLETE_SSL_NONE ==
         net::ObsoleteSSLStatus(
             net::SSLConnectionStatusToCipherSuite(ssl.connection_status)))) {
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT, subject_name));
    } else {
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
          subject_name));
    }
    if (ssl.content_status) {
      bool ran_insecure_content = false;  // Always false on iOS.
      icon_id = ran_insecure_content ? ICON_STATE_ERROR : ICON_NONE;
      description.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK, description,
          l10n_util::GetStringUTF16(
              ran_insecure_content
                  ? IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_ERROR
                  : IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING)));
    }
  }

  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(ssl.connection_status);
  if (ssl.security_bits > 0 && cipher_suite) {
    int ssl_version = net::SSLConnectionStatusToVersion(ssl.connection_status);
    const char* ssl_version_str;
    net::SSLVersionToString(&ssl_version_str, ssl_version);
    description += base::ASCIIToUTF16("\n\n");
    description +=
        l10n_util::GetStringFUTF16(IDS_PAGE_INFO_SECURITY_TAB_SSL_VERSION,
                                   base::ASCIIToUTF16(ssl_version_str));

    const char *key_exchange, *cipher, *mac;
    bool is_aead;
    bool is_tls13;
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);

    description += base::ASCIIToUTF16("\n\n");
    if (is_aead) {
      description += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS_AEAD,
          base::ASCIIToUTF16(cipher), base::ASCIIToUTF16(key_exchange));
    } else {
      description += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS,
          base::ASCIIToUTF16(cipher), base::ASCIIToUTF16(mac),
          base::ASCIIToUTF16(key_exchange));
    }
  }

  if (!description.empty()) {
    sections_.push_back(SectionInfo(icon_id, headline, description,
                                    SECTION_INFO_CONNECTION,
                                    BUTTON_SHOW_SECURITY_HELP));
  }

  if (ssl.certificate) {
    certificate_label_ =
        l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON);
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
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  switch (icon_id) {
    case ICON_NONE:
    case ICON_STATE_INTERNAL_PAGE:
      return nullptr;
    case ICON_STATE_OK:
      return &rb.GetNativeImageNamed(IDR_IOS_PAGEINFO_GOOD);
    case ICON_STATE_ERROR:
      return &rb.GetNativeImageNamed(IDR_IOS_PAGEINFO_BAD);
    case ICON_STATE_INFO:
      return &rb.GetNativeImageNamed(IDR_IOS_PAGEINFO_INFO);
    case ICON_STATE_OFFLINE_PAGE:
      return &rb.GetNativeImageNamed(IDR_IOS_OMNIBOX_OFFLINE);
  }
}

base::string16 PageInfoModel::GetCertificateLabel() const {
  return certificate_label_;
}

PageInfoModel::PageInfoModel() : observer_(NULL) {}
