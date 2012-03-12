// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/website_settings_model.h"

#include <string>
#include <vector>

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "content/public/browser/cert_store.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_cipher_suite_names.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

WebsiteSettingsModel::WebsiteSettingsModel(Profile* profile,
                                           const GURL& url,
                                           const content::SSLStatus& ssl,
                                           content::CertStore* cert_store)
    : site_identity_status_(SITE_IDENTITY_STATUS_UNKNOWN),
      site_connection_status_(SITE_CONNECTION_STATUS_UNKNOWN),
      cert_store_(cert_store)  {
  Init(profile, url, ssl);
  // After initialization the status about the site's connection
  // and it's identity must be available.
  DCHECK_NE(site_identity_status_, SITE_IDENTITY_STATUS_UNKNOWN);
  DCHECK_NE(site_connection_status_, SITE_CONNECTION_STATUS_UNKNOWN);
}

WebsiteSettingsModel::~WebsiteSettingsModel() {
}

void WebsiteSettingsModel::Init(Profile* profile,
                                const GURL& url,
                                const content::SSLStatus& ssl) {
  if (url.SchemeIs(chrome::kChromeUIScheme)) {
    site_identity_status_ = SITE_IDENTITY_STATUS_INTERNAL_PAGE;
    site_identity_details_ =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE);
    site_connection_status_ = SITE_CONNECTION_STATUS_INTERNAL_PAGE;
    return;
  }

  scoped_refptr<net::X509Certificate> cert;

  // Identity section.
  string16 subject_name(UTF8ToUTF16(url.host()));
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
  }

  if (ssl.cert_id &&
      cert_store_->RetrieveCert(ssl.cert_id, &cert) &&
      (!net::IsCertStatusError(ssl.cert_status) ||
       net::IsCertStatusMinorError(ssl.cert_status))) {
    // There are no major errors. Check for minor errors.
    if (net::IsCertStatusMinorError(ssl.cert_status)) {
      site_identity_status_ = SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN;
      string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }
      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));

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
      site_identity_status_ = SITE_IDENTITY_STATUS_EV_CERT;
      DCHECK(!cert->subject().organization_names.empty());
      organization_name_ = UTF8ToUTF16(cert->subject().organization_names[0]);
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
      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV,
          UTF8ToUTF16(cert->subject().organization_names[0]),
          locality,
          UTF8ToUTF16(cert->issuer().GetDisplayName())));
    } else if (ssl.cert_status & net::CERT_STATUS_IS_DNSSEC) {
      // DNSSEC authenticated page.
      site_identity_status_ = SITE_IDENTITY_STATUS_DNSSEC_CERT;
      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, UTF8ToUTF16("DNSSEC")));
    } else {
      // Non-EV OK HTTPS page.
      site_identity_status_ = SITE_IDENTITY_STATUS_CERT;
      string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      }
      site_identity_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    site_identity_details_.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    if (ssl.security_style == content::SECURITY_STYLE_UNAUTHENTICATED)
      site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
    else
      site_identity_status_ = SITE_IDENTITY_STATUS_ERROR;

    const string16 bullet = UTF8ToUTF16("\n • ");
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

  if (!ssl.cert_id) {
    // Not HTTPS.
    DCHECK_EQ(ssl.security_style, content::SECURITY_STYLE_UNAUTHENTICATED);
    if (ssl.security_style == content::SECURITY_STYLE_UNAUTHENTICATED)
      site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;
    else
      site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;

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
                         net::SSL_CONNECTION_SSL3_FALLBACK) != 0;
    bool no_renegotiation =
        (ssl.connection_status &
        net::SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION) != 0;
    const char *key_exchange, *cipher, *mac;
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, cipher_suite);

    site_connection_details_ += ASCIIToUTF16("\n\n");
    site_connection_details_ += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS,
        ASCIIToUTF16(cipher), ASCIIToUTF16(mac), ASCIIToUTF16(key_exchange));

    site_connection_details_ += ASCIIToUTF16("\n\n");
    uint8 compression_id =
        net::SSLConnectionStatusToCompression(ssl.connection_status);
    if (compression_id) {
      const char* compression;
      net::SSLCompressionToString(&compression, compression_id);
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_COMPRESSION_DETAILS,
          ASCIIToUTF16(compression));
    } else {
      site_connection_details_ += l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NO_COMPRESSION);
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
}
