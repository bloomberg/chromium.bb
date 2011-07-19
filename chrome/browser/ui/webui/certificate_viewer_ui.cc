// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_viewer_ui.h"

#include "base/string_number_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/browser/cert_store.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

CertificateViewerUI::CertificateViewerUI(TabContents* contents)
    : ChromeWebUI(contents) {

  // Set up the chrome://view-cert source.
  ChromeWebUIDataSource* html_source =
      new ChromeWebUIDataSource(chrome::kChromeUICertificateViewerHost);

  // Register callback handler to retrieve certificate information.
  RegisterMessageCallback("requestCertificateInfo",
      NewCallback(this, &CertificateViewerUI::RequestCertificateInfo));

  // Localized strings.
  html_source->AddLocalizedString("close", IDS_CLOSE);
  html_source->AddLocalizedString("usages",
      IDS_CERT_INFO_VERIFIED_USAGES_GROUP);
  html_source->AddLocalizedString("issuedto", IDS_CERT_INFO_SUBJECT_GROUP);
  html_source->AddLocalizedString("issuedby", IDS_CERT_INFO_ISSUER_GROUP);
  html_source->AddLocalizedString("cn", IDS_CERT_INFO_COMMON_NAME_LABEL);
  html_source->AddLocalizedString("o", IDS_CERT_INFO_ORGANIZATION_LABEL);
  html_source->AddLocalizedString("ou",
      IDS_CERT_INFO_ORGANIZATIONAL_UNIT_LABEL);
  html_source->AddLocalizedString("sn", IDS_CERT_INFO_SERIAL_NUMBER_LABEL);
  html_source->AddLocalizedString("validity", IDS_CERT_INFO_VALIDITY_GROUP);
  html_source->AddLocalizedString("issuedon", IDS_CERT_INFO_ISSUED_ON_LABEL);
  html_source->AddLocalizedString("expireson", IDS_CERT_INFO_EXPIRES_ON_LABEL);
  html_source->AddLocalizedString("fingerprints",
      IDS_CERT_INFO_FINGERPRINTS_GROUP);
  html_source->AddLocalizedString("sha256",
      IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL);
  html_source->AddLocalizedString("sha1", IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL);
  html_source->set_json_path("strings.js");

  // Add required resources.
  html_source->add_resource_path("certificate_viewer.js",
      IDR_CERTIFICATE_VIEWER_JS);
  html_source->add_resource_path("certificate_viewer.css",
      IDR_CERTIFICATE_VIEWER_CSS);
  html_source->set_default_resource(IDR_CERTIFICATE_VIEWER_HTML);

  contents->profile()->GetChromeURLDataManager()->AddDataSource(
      html_source);
}

CertificateViewerUI::~CertificateViewerUI() {
}

// TODO(flackr): This is duplicated from cookies_view_handler.cc
// Decodes a pointer from a hex string.
void* HexStringToPointer(const std::string& str) {
  std::vector<uint8> buffer;
  if (!base::HexStringToBytes(str, &buffer) ||
      buffer.size() != sizeof(void*)) {
    return NULL;
  }

  return *reinterpret_cast<void**>(&buffer[0]);
}

// Returns the certificate information of the requested certificate id from
// the CertStore to the javascript handler.
void CertificateViewerUI::RequestCertificateInfo(const ListValue* args) {
  // The certificate id should be in the first argument.
  std::string val;
  if (!(args->GetString(0, &val))) {
    return;
  }
  net::X509Certificate* cert = static_cast<net::X509Certificate*>(
      HexStringToPointer(val));

  // Certificate information. The keys in this dictionary correspond to the IDs
  // in the Html page.
  DictionaryValue cert_info;
  net::X509Certificate::OSCertHandle cert_hnd = cert->os_cert_handle();

  // Get the certificate chain.
  net::X509Certificate::OSCertHandles cert_chain;
  x509_certificate_model::GetCertChainFromCert(cert_hnd, &cert_chain);

  // Certificate usage.
  std::vector<std::string> usages;
  x509_certificate_model::GetUsageStrings(cert_hnd, &usages);
  std::string usagestr;
  for (std::vector<std::string>::iterator it = usages.begin();
      it != usages.end(); ++it) {
    if (usagestr.length() > 0) {
      usagestr += "\n";
    }
    usagestr += *it;
  }
  cert_info.SetString("usages", usagestr);

  // Standard certificate details.
  const std::string alternative_text =
      l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT);
  cert_info.SetString("title", l10n_util::GetStringFUTF8(
      IDS_CERT_INFO_DIALOG_TITLE, UTF8ToUTF16(x509_certificate_model::GetTitle(
          cert_chain.front()))));

  // Issued to information.
  cert_info.SetString("issued-cn", x509_certificate_model::GetSubjectCommonName(
      cert_hnd, alternative_text));
  cert_info.SetString("issued-o", x509_certificate_model::GetSubjectOrgName(
      cert_hnd, alternative_text));
  cert_info.SetString("issued-ou", x509_certificate_model::GetSubjectOrgUnitName
      (cert_hnd, alternative_text));
  cert_info.SetString("issued-sn",
      x509_certificate_model::GetSerialNumberHexified(
          cert_hnd, alternative_text));

  // Issuer information.
  cert_info.SetString("issuer-cn", x509_certificate_model::GetIssuerCommonName(
      cert_hnd, alternative_text));
  cert_info.SetString("issuer-o", x509_certificate_model::GetIssuerOrgName(
      cert_hnd, alternative_text));
  cert_info.SetString("issuer-ou", x509_certificate_model::GetIssuerOrgUnitName
      (cert_hnd, alternative_text));

  // Validity period.
  base::Time issued, expires;
  std::string issued_str, expires_str;
  if (x509_certificate_model::GetTimes(cert_hnd, &issued, &expires)) {
    issued_str = UTF16ToUTF8(
        base::TimeFormatShortDateNumeric(issued));
    expires_str = UTF16ToUTF8(
        base::TimeFormatShortDateNumeric(expires));
  } else {
    issued_str = alternative_text;
    expires_str = alternative_text;
  }
  cert_info.SetString("issuedate", issued_str);
  cert_info.SetString("expirydate", expires_str);

  cert_info.SetString("sha256",
      x509_certificate_model::HashCertSHA256(cert_hnd));
  cert_info.SetString("sha1",
      x509_certificate_model::HashCertSHA1(cert_hnd));

  // Send certificate information to javascript.
  CallJavascriptFunction("cert_viewer.getCertificateInfo", cert_info);
}

