// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_viewer_webui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;
using web_modal::NativeWebContentsModalDialog;

// Shows a certificate using the WebUI certificate viewer.
void ShowCertificateViewer(WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  CertificateViewerDialog* dialog = new CertificateViewerDialog(cert);
  dialog->Show(web_contents, parent);
}

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialog

CertificateViewerModalDialog::CertificateViewerModalDialog(
    net::X509Certificate* cert)
    : cert_(cert), webui_(NULL), window_(NULL) {
  // Construct the dialog title from the certificate.
  title_ = l10n_util::GetStringFUTF16(
      IDS_CERT_INFO_DIALOG_TITLE,
      base::UTF8ToUTF16(
          x509_certificate_model::GetTitle(cert_->os_cert_handle())));
}

CertificateViewerModalDialog::~CertificateViewerModalDialog() {
}

void CertificateViewerModalDialog::Show(content::WebContents* web_contents,
                                        gfx::NativeWindow parent) {
  window_ = chrome::ShowWebDialog(parent,
                                  web_contents->GetBrowserContext(),
                                  this);
}

NativeWebContentsModalDialog
CertificateViewerModalDialog::GetNativeWebContentsModalDialog() {
#if defined(USE_AURA)
  return window_;
#else
  NOTREACHED();
  return NULL;
#endif
}

ui::ModalType CertificateViewerModalDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 CertificateViewerModalDialog::GetDialogTitle() const {
  return title_;
}

GURL CertificateViewerModalDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUICertificateViewerDialogURL);
}

void CertificateViewerModalDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(new CertificateViewerDialogHandler(
      const_cast<CertificateViewerModalDialog*>(this), cert_.get()));
}

void CertificateViewerModalDialog::GetDialogSize(gfx::Size* size) const {
  const int kDefaultWidth = 544;
  const int kDefaultHeight = 628;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string CertificateViewerModalDialog::GetDialogArgs() const {
  std::string data;

  // Certificate information. The keys in this dictionary's general key
  // correspond to the IDs in the Html page.
  base::DictionaryValue cert_info;
  net::X509Certificate::OSCertHandle cert_hnd = cert_->os_cert_handle();

  // Get the certificate chain.
  net::X509Certificate::OSCertHandles cert_chain;
  cert_chain.push_back(cert_->os_cert_handle());
  const net::X509Certificate::OSCertHandles& certs =
      cert_->GetIntermediateCertificates();
  cert_chain.insert(cert_chain.end(), certs.begin(), certs.end());

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
      IDS_CERT_INFO_DIALOG_TITLE,
      base::UTF8ToUTF16(x509_certificate_model::GetTitle(
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
    issued_str = base::UTF16ToUTF8(
        base::TimeFormatShortDateNumeric(issued));
    expires_str = base::UTF16ToUTF8(
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
  base::ListValue* children = NULL;
  int index = 0;
  for (net::X509Certificate::OSCertHandles::const_iterator i =
      cert_chain.begin(); i != cert_chain.end(); ++i, ++index) {
    base::DictionaryValue* cert_node = new base::DictionaryValue();
    base::ListValue cert_details;
    cert_node->SetString("label", x509_certificate_model::GetTitle(*i).c_str());
    cert_node->SetDouble("payload.index", index);
    // Add the child from the previous iteration.
    if (children)
      cert_node->Set("children", children);

    // Add this node to the children list for the next iteration.
    children = new base::ListValue();
    children->Append(cert_node);
  }
  // Set the last node as the top of the certificate hierarchy.
  cert_info.Set("hierarchy", children);

  base::JSONWriter::Write(&cert_info, &data);

  return data;
}

void CertificateViewerModalDialog::OnDialogShown(
    content::WebUI* webui,
    content::RenderViewHost* render_view_host) {
  webui_ = webui;
}

void CertificateViewerModalDialog::OnDialogClosed(
    const std::string& json_retval) {
}

void CertificateViewerModalDialog::OnCloseContents(WebContents* source,
                                              bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool CertificateViewerModalDialog::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialog

CertificateViewerDialog::CertificateViewerDialog(net::X509Certificate* cert)
    : CertificateViewerModalDialog(cert),
      dialog_(NULL) {
}

CertificateViewerDialog::~CertificateViewerDialog() {
}

void CertificateViewerDialog::Show(WebContents* web_contents,
                                   gfx::NativeWindow parent) {
  // TODO(bshe): UI tweaks needed for Aura HTML Dialog, such as adding padding
  // on the title for Aura ConstrainedWebDialogUI.
  dialog_ = CreateConstrainedWebDialog(
      web_contents->GetBrowserContext(),
      this,
      NULL,
      web_contents);
}

NativeWebContentsModalDialog
CertificateViewerDialog::GetNativeWebContentsModalDialog() {
  return dialog_->GetNativeDialog();
}

GURL CertificateViewerDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUICertificateViewerURL);
}

ui::ModalType CertificateViewerDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialogHandler

CertificateViewerDialogHandler::CertificateViewerDialogHandler(
    CertificateViewerModalDialog* dialog,
    net::X509Certificate* cert)
    : cert_(cert), dialog_(dialog) {
  cert_chain_.push_back(cert_->os_cert_handle());
  const net::X509Certificate::OSCertHandles& certs =
      cert_->GetIntermediateCertificates();
  cert_chain_.insert(cert_chain_.end(), certs.begin(), certs.end());
}

CertificateViewerDialogHandler::~CertificateViewerDialogHandler() {
}

void CertificateViewerDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("exportCertificate",
      base::Bind(&CertificateViewerDialogHandler::ExportCertificate,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestCertificateFields",
      base::Bind(&CertificateViewerDialogHandler::RequestCertificateFields,
                 base::Unretained(this)));
}

void CertificateViewerDialogHandler::ExportCertificate(
    const base::ListValue* args) {
  int cert_index = GetCertificateIndex(args);
  if (cert_index < 0)
    return;

  NativeWebContentsModalDialog window =
      platform_util::GetTopLevel(dialog_->GetNativeWebContentsModalDialog());
  ShowCertExportDialog(web_ui()->GetWebContents(),
                       window,
                       cert_chain_.begin() + cert_index,
                       cert_chain_.end());
}

void CertificateViewerDialogHandler::RequestCertificateFields(
    const base::ListValue* args) {
  int cert_index = GetCertificateIndex(args);
  if (cert_index < 0)
    return;

  net::X509Certificate::OSCertHandle cert = cert_chain_[cert_index];

  base::ListValue root_list;
  base::DictionaryValue* node_details;
  base::DictionaryValue* alt_node_details;
  base::ListValue* cert_sub_fields;
  root_list.Append(node_details = new base::DictionaryValue());
  node_details->SetString("label", x509_certificate_model::GetTitle(cert));

  base::ListValue* cert_fields;
  node_details->Set("children", cert_fields = new base::ListValue());
  cert_fields->Append(node_details = new base::DictionaryValue());

  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE));
  node_details->Set("children", cert_fields = new base::ListValue());

  // Main certificate fields.
  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_VERSION));
  std::string version = x509_certificate_model::GetVersion(cert);
  if (!version.empty())
    node_details->SetString("payload.val",
        l10n_util::GetStringFUTF8(IDS_CERT_DETAILS_VERSION_FORMAT,
                                  base::UTF8ToUTF16(version)));

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SERIAL_NUMBER));
  node_details->SetString("payload.val",
      x509_certificate_model::GetSerialNumberHexified(cert,
          l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT)));

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSecAlgorithmSignature(cert));

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_ISSUER));
  node_details->SetString("payload.val",
      x509_certificate_model::GetIssuerName(cert));

  // Validity period.
  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_VALIDITY));

  node_details->Set("children", cert_sub_fields = new base::ListValue());
  cert_sub_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_NOT_BEFORE));
  cert_sub_fields->Append(alt_node_details = new base::DictionaryValue());
  alt_node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_NOT_AFTER));
  base::Time issued, expires;
  if (x509_certificate_model::GetTimes(cert, &issued, &expires)) {
    // The object Time internally saves the time in UTC timezone. This is why we
    // do a simple UTC string concatenation.
    node_details->SetString(
        "payload.val",
        base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(issued)) + " " +
            l10n_util::GetStringUTF8(IDS_CERT_DETAILS_UTC_TIMEZONE));
    alt_node_details->SetString(
        "payload.val",
        base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(expires)) + " " +
            l10n_util::GetStringUTF8(IDS_CERT_DETAILS_UTC_TIMEZONE));
  }

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT));
  node_details->SetString("payload.val",
      x509_certificate_model::GetSubjectName(cert));

  // Subject key information.
  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT_KEY_INFO));

  node_details->Set("children", cert_sub_fields = new base::ListValue());
  cert_sub_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_SUBJECT_KEY_ALG));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSecAlgorithmSubjectPublicKey(cert));
  cert_sub_fields->Append(node_details = new base::DictionaryValue());
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
    cert_fields->Append(node_details = new base::DictionaryValue());
    node_details->SetString("label",
        l10n_util::GetStringUTF8(IDS_CERT_DETAILS_EXTENSIONS));

    node_details->Set("children", cert_sub_fields = new base::ListValue());
    for (x509_certificate_model::Extensions::const_iterator i =
         extensions.begin(); i != extensions.end(); ++i) {
      cert_sub_fields->Append(node_details = new base::DictionaryValue());
      node_details->SetString("label", i->name);
      node_details->SetString("payload.val", i->value);
    }
  }

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessSecAlgorithmSignatureWrap(cert));

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE_SIG_VALUE));
  node_details->SetString("payload.val",
      x509_certificate_model::ProcessRawBitsSignatureWrap(cert));

  cert_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_INFO_FINGERPRINTS_GROUP));
  node_details->Set("children", cert_sub_fields = new base::ListValue());

  cert_sub_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL));
  node_details->SetString("payload.val",
      x509_certificate_model::HashCertSHA256(cert));
  cert_sub_fields->Append(node_details = new base::DictionaryValue());
  node_details->SetString("label",
      l10n_util::GetStringUTF8(IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL));
  node_details->SetString("payload.val",
      x509_certificate_model::HashCertSHA1(cert));

  // Send certificate information to javascript.
  web_ui()->CallJavascriptFunction("cert_viewer.getCertificateFields",
      root_list);
}

int CertificateViewerDialogHandler::GetCertificateIndex(
    const base::ListValue* args) const {
  int cert_index;
  double val;
  if (!(args->GetDouble(0, &val)))
    return -1;
  cert_index = static_cast<int>(val);
  if (cert_index < 0 || cert_index >= static_cast<int>(cert_chain_.size()))
    return -1;
  return cert_index;
}
