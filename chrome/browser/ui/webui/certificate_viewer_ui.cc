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
    : HtmlDialogUI(contents) {

  // Set up the chrome://view-cert source.
  ChromeWebUIDataSource* html_source =
      new ChromeWebUIDataSource(chrome::kChromeUICertificateViewerHost);

  // Register callback handler to retrieve certificate information.
  RegisterMessageCallback("requestCertificateInfo",
      NewCallback(this, &CertificateViewerUI::RequestCertificateInfo));

  // Localized strings.
  html_source->AddLocalizedString("general", IDS_CERT_INFO_GENERAL_TAB_LABEL);
  html_source->AddLocalizedString("details", IDS_CERT_INFO_DETAILS_TAB_LABEL);
  html_source->AddLocalizedString("close", IDS_CLOSE);
  html_source->AddLocalizedString("usages",
      IDS_CERT_INFO_VERIFIED_USAGES_GROUP);
  html_source->AddLocalizedString("issuedTo", IDS_CERT_INFO_SUBJECT_GROUP);
  html_source->AddLocalizedString("issuedBy", IDS_CERT_INFO_ISSUER_GROUP);
  html_source->AddLocalizedString("cn", IDS_CERT_INFO_COMMON_NAME_LABEL);
  html_source->AddLocalizedString("o", IDS_CERT_INFO_ORGANIZATION_LABEL);
  html_source->AddLocalizedString("ou",
      IDS_CERT_INFO_ORGANIZATIONAL_UNIT_LABEL);
  html_source->AddLocalizedString("sn", IDS_CERT_INFO_SERIAL_NUMBER_LABEL);
  html_source->AddLocalizedString("validity", IDS_CERT_INFO_VALIDITY_GROUP);
  html_source->AddLocalizedString("issuedOn", IDS_CERT_INFO_ISSUED_ON_LABEL);
  html_source->AddLocalizedString("expiresOn", IDS_CERT_INFO_EXPIRES_ON_LABEL);
  html_source->AddLocalizedString("fingerprints",
      IDS_CERT_INFO_FINGERPRINTS_GROUP);
  html_source->AddLocalizedString("sha256",
      IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL);
  html_source->AddLocalizedString("sha1", IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL);
  html_source->AddLocalizedString("hierarchy",
      IDS_CERT_DETAILS_CERTIFICATE_HIERARCHY_LABEL);
  html_source->AddLocalizedString("certFields",
      IDS_CERT_DETAILS_CERTIFICATE_FIELDS_LABEL);
  html_source->AddLocalizedString("certFieldVal",
      IDS_CERT_DETAILS_CERTIFICATE_FIELD_VALUE_LABEL);
  html_source->set_json_path("strings.js");

  // Add required resources.
  html_source->add_resource_path("certificate_viewer.js",
      IDR_CERTIFICATE_VIEWER_JS);
  html_source->add_resource_path("certificate_viewer.css",
      IDR_CERTIFICATE_VIEWER_CSS);
  html_source->set_default_resource(IDR_CERTIFICATE_VIEWER_HTML);

  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
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

  // Certificate information. The keys in this dictionary's general key
  // correspond to the IDs in the Html page.
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
  cert_info.SetString("general.usages", usagestr);

  // Standard certificate details.
  const std::string alternative_text =
      l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT);
  cert_info.SetString("general.title", l10n_util::GetStringFUTF8(
      IDS_CERT_INFO_DIALOG_TITLE, UTF8ToUTF16(x509_certificate_model::GetTitle(
          cert_chain.front()))));

  // Issued to information.
  cert_info.SetString("general.issued-cn",
      x509_certificate_model::GetSubjectCommonName(cert_hnd, alternative_text));
  cert_info.SetString("general.issued-o",
      x509_certificate_model::GetSubjectOrgName(cert_hnd, alternative_text));
  cert_info.SetString("general.issued-ou",
      x509_certificate_model::GetSubjectOrgUnitName(cert_hnd,
                                                    alternative_text));
  cert_info.SetString("general.issued-sn",
      x509_certificate_model::GetSerialNumberHexified(cert_hnd,
                                                      alternative_text));

  // Issuer information.
  cert_info.SetString("general.issuer-cn",
      x509_certificate_model::GetIssuerCommonName(cert_hnd, alternative_text));
  cert_info.SetString("general.issuer-o",
      x509_certificate_model::GetIssuerOrgName(cert_hnd, alternative_text));
  cert_info.SetString("general.issuer-ou",
      x509_certificate_model::GetIssuerOrgUnitName(cert_hnd, alternative_text));

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
  cert_info.SetString("general.issue-date", issued_str);
  cert_info.SetString("general.expiry-date", expires_str);

  cert_info.SetString("general.sha256",
      x509_certificate_model::HashCertSHA256(cert_hnd));
  cert_info.SetString("general.sha1",
      x509_certificate_model::HashCertSHA1(cert_hnd));

  // Certificate hierarchy is constructed from bottom up.
  ListValue* children = NULL;
  for (net::X509Certificate::OSCertHandles::const_iterator i =
      cert_chain.begin(); i != cert_chain.end(); ++i) {
    DictionaryValue* cert_node = new DictionaryValue();
    ListValue cert_details;
    cert_node->SetString("label", x509_certificate_model::GetTitle(*i).c_str());
    cert_node->Set("payload.fields", GetCertificateFields(*i));
    // Add the child from the previous iteration.
    if (children)
      cert_node->Set("children", children);

    // Add this node to the children list for the next iteration.
    children = new ListValue();
    children->Append(cert_node);
  }
  // Set the last node as the top of the certificate hierarchy.
  cert_info.Set("hierarchy", children);

  // Send certificate information to javascript.
  CallJavascriptFunction("cert_viewer.getCertificateInfo", cert_info);
}

ListValue* CertificateViewerUI::GetCertificateFields(
    net::X509Certificate::OSCertHandle cert) {
  ListValue* root_list = new ListValue();
  DictionaryValue* node_details;
  DictionaryValue* alt_node_details;
  ListValue* cert_sub_fields;
  root_list->Append(node_details = new DictionaryValue());
  node_details->SetString("label", x509_certificate_model::GetTitle(cert));

  ListValue* cert_fields;
  node_details->Set("children", cert_fields = new ListValue());
  cert_fields->Append(node_details = new DictionaryValue());

  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE));
  node_details->Set("children", cert_fields = new ListValue());

  // Main certificate fields.
  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_VERSION));
  std::string version = x509_certificate_model::GetVersion(cert);
  if (!version.empty())
    node_details->SetString("payload.val",
        l10n_util::GetStringFUTF8(IDS_CERT_DETAILS_VERSION_FORMAT,
                                  UTF8ToUTF16(version)));

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SERIAL_NUMBER));
  node_details->SetString("payload.val",
      x509_certificate_model::GetSerialNumberHexified(cert,
          l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT)));

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSecAlgorithmSignature(cert));

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_ISSUER));
  node_details->SetString("payload.val",
      x509_certificate_model::GetIssuerName(cert));

  // Validity period.
  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_VALIDITY));

  node_details->Set("children", cert_sub_fields = new ListValue());
  cert_sub_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_NOT_BEFORE));
  cert_sub_fields->Append(alt_node_details = new DictionaryValue());
  alt_node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_NOT_AFTER));
  base::Time issued, expires;
  if (x509_certificate_model::GetTimes(cert, &issued, &expires)) {
    node_details->SetString("payload.val",
        UTF16ToUTF8(base::TimeFormatShortDateAndTime(issued)));
    alt_node_details->SetString("payload.val",
        UTF16ToUTF8(base::TimeFormatShortDateAndTime(expires)));
  }

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT));
  node_details->SetString("payload.val",
      x509_certificate_model::GetSubjectName(cert));

  // Subject key information.
  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT_KEY_INFO));

  node_details->Set("children", cert_sub_fields = new ListValue());
  cert_sub_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT_KEY_ALG));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSecAlgorithmSubjectPublicKey(cert));
  cert_sub_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT_KEY));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSubjectPublicKeyInfo(cert));

  // Extensions.
  x509_certificate_model::Extensions extensions;
  x509_certificate_model::GetExtensions(
      l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_CRITICAL),
      l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_NON_CRITICAL),
      cert, &extensions);

  if (!extensions.empty()) {
    cert_fields->Append(node_details = new DictionaryValue());
    node_details->SetString("label",
        l10n_util::GetStringUTF8(IDS_CERT_DETAILS_EXTENSIONS));

    node_details->Set("children", cert_sub_fields = new ListValue());
    for (x509_certificate_model::Extensions::const_iterator i =
         extensions.begin(); i != extensions.end(); ++i) {
      cert_sub_fields->Append(node_details = new DictionaryValue());
      node_details->SetString("label", i->name);
      node_details->SetString("payload.val", i->value);
    }
  }

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSecAlgorithmSignatureWrap(cert));

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE_SIG_VALUE));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessRawBitsSignatureWrap(cert));

  cert_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_INFO_FINGERPRINTS_GROUP));
  node_details->Set("children", cert_sub_fields = new ListValue());

  cert_sub_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL));
  node_details->SetString("payload.val",
      x509_certificate_model::HashCertSHA256(cert));
  cert_sub_fields->Append(node_details = new DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL));
  node_details->SetString("payload.val",
      x509_certificate_model::HashCertSHA1(cert));
  return root_list;
}

