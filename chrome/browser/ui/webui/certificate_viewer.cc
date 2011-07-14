// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/ui/webui/certificate_viewer.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "grit/generated_resources.h"

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 450;
const int kDefaultHeight = 450;

}  // namespace

// Shows a certificate using the WebUI certificate viewer.
void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  CertificateViewerDialog::ShowDialog(parent, cert);
}

void CertificateViewerDialog::ShowDialog(gfx::NativeWindow owning_window,
                                         net::X509Certificate* cert) {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->BrowserShowHtmlDialog(new CertificateViewerDialog(cert),
                                 owning_window);
}

// TODO(flackr): This is duplicated from cookies_view_handler.cc
// Encodes a pointer value into a hex string.
std::string PointerToHexString(const void* pointer) {
  return base::HexEncode(&pointer, sizeof(pointer));
}

CertificateViewerDialog::CertificateViewerDialog(net::X509Certificate* cert)
    : cert_(cert) {
  // Construct the certificate viewer URL to view the stored certificate.
  // TODO(flackr): The certificate pointer could be passed through DialogArgs,
  //     however GetDialogArgs is currently not called in Linux builds. When
  //     GTK dialogs are fixed or replaced with Views move the certificate
  //     pointer to DialogArgs to remove the potential of entering a URL which
  //     may crash the browser.
  GURL::Replacements replacements;
  std::string query_string = "cert=" + PointerToHexString(cert_);
  replacements.SetQuery(query_string.c_str(), url_parse::Component(0,
      query_string.length()));
  dialogURL_ = GURL(chrome::kChromeUICertificateViewerURL).
      ReplaceComponents(replacements);

  // Construct the dialog title from the certificate.
  net::X509Certificate::OSCertHandles cert_chain;
  x509_certificate_model::GetCertChainFromCert(cert_->os_cert_handle(),
      &cert_chain);
  title_ = UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_CERT_INFO_DIALOG_TITLE, UTF8ToUTF16(x509_certificate_model::GetTitle(
          cert_chain.front()))));
}

bool CertificateViewerDialog::IsDialogModal() const {
  return false;
}

std::wstring CertificateViewerDialog::GetDialogTitle() const {
  return title_;
}

GURL CertificateViewerDialog::GetDialogContentURL() const {
  return dialogURL_;
}

void CertificateViewerDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void CertificateViewerDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string CertificateViewerDialog::GetDialogArgs() const {
  return std::string();
}

void CertificateViewerDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void CertificateViewerDialog::OnCloseContents(TabContents* source,
                                              bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool CertificateViewerDialog::ShouldShowDialogTitle() const {
  return true;
}

bool CertificateViewerDialog::HandleContextMenu(
    const ContextMenuParams& params) {
  return true;
}
