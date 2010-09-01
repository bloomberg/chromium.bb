// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info_model.h"

#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_cipher_suite_names.h"
#include "net/base/x509_certificate.h"

PageInfoModel::PageInfoModel(Profile* profile,
                             const GURL& url,
                             const NavigationEntry::SSLStatus& ssl,
                             bool show_history,
                             PageInfoModelObserver* observer)
    : observer_(observer) {
  SectionInfoState state = SECTION_STATE_OK;
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
      CertStore::GetSharedInstance()->RetrieveCert(ssl.cert_id(), &cert) &&
      !net::IsCertStatusError(ssl.cert_status())) {
    // OK HTTPS page.
    if ((ssl.cert_status() & net::CERT_STATUS_IS_EV) != 0) {
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
    } else {
      // Non EV OK HTTPS.
      if (empty_subject_name)
        headline.clear();  // Don't display any title.
      else
        headline.assign(subject_name);
      string16 issuer_name(UTF8ToUTF16(cert->issuer().GetDisplayName()));
      if (issuer_name.empty()) {
        issuer_name.assign(l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      } else {
        description.assign(l10n_util::GetStringFUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));
      }
    }
  } else {
    // HTTP or bad HTTPS.
    description.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    state = SECTION_STATE_ERROR;
  }
  sections_.push_back(SectionInfo(
      state,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_IDENTITY_TITLE),
      headline,
      description,
      SECTION_INFO_IDENTITY));

  // Connection section.
  // We consider anything less than 80 bits encryption to be weak encryption.
  // TODO(wtc): Bug 1198735: report mixed/unsafe content for unencrypted and
  // weakly encrypted connections.
  state = SECTION_STATE_OK;
  headline.clear();
  description.clear();
  if (ssl.security_bits() <= 0) {
    state = SECTION_STATE_ERROR;
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (ssl.security_bits() < 80) {
    state = SECTION_STATE_ERROR;
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
        subject_name));
  } else {
    description.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
        subject_name,
        base::IntToString16(ssl.security_bits())));
    if (ssl.displayed_insecure_content() || ssl.ran_insecure_content()) {
      const CommandLine* command_line(CommandLine::ForCurrentProcess());
      if (command_line->HasSwitch(switches::kEnableNewPageInfoBubble))
        state = SECTION_STATE_WARNING;
      else
        state = SECTION_STATE_ERROR;
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
      const char *compression;
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
      state = SECTION_STATE_ERROR;
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

  sections_.push_back(SectionInfo(
      state,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_CONNECTION_TITLE),
      headline,
      description,
      SECTION_INFO_CONNECTION));

  // Request the number of visits.
  HistoryService* history = profile->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (show_history && history) {
    history->GetVisitCountToHost(
        url,
        &request_consumer_,
        NewCallback(this, &PageInfoModel::OnGotVisitCountToHost));
  }
}

PageInfoModel::~PageInfoModel() {
}

int PageInfoModel::GetSectionCount() {
  return sections_.size();
}

PageInfoModel::SectionInfo PageInfoModel::GetSectionInfo(int index) {
  DCHECK(index < static_cast<int>(sections_.size()));
  return sections_[index];
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

  if (!visited_before_today) {
    sections_.push_back(SectionInfo(
        SECTION_STATE_ERROR,
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_PERSONAL_HISTORY_TITLE),
        string16(),
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_FIRST_VISITED_TODAY),
        SECTION_INFO_FIRST_VISIT));
  } else {
    sections_.push_back(SectionInfo(
        SECTION_STATE_OK,
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_PERSONAL_HISTORY_TITLE),
        string16(),
        l10n_util::GetStringFUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_VISITED_BEFORE_TODAY,
            WideToUTF16(base::TimeFormatShortDate(first_visit))),
        SECTION_INFO_FIRST_VISIT));
  }
  observer_->ModelChanged();
}

// static
void PageInfoModel::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kPageInfoWindowPlacement);
}

PageInfoModel::PageInfoModel() {
}
