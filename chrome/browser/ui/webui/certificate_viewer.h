// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_H_
#pragma once

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "net/base/x509_certificate.h"
#include "ui/gfx/native_widget_types.h"

// Displays the WebUI certificate viewer dialog for the passed in certificate.
void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate*);

// Dialog for displaying detailed certificate information. This is used in linux
// and chromeos builds to display detailed information in a floating dialog when
// the user clicks on "Certificate Information" from the lock icon of a web site
// or "View" from the Certificate Manager.
class CertificateViewerDialog : private HtmlDialogUIDelegate {
 public:
  // Shows the certificate viewer dialog for the passed in certificate.
  static void ShowDialog(gfx::NativeWindow owning_window,
                         net::X509Certificate* cert);

 private:
  // Construct a certificate viewer for the passed in certificate. A reference
  // to the certificate pointer is added for the lifetime of the certificate
  // viewer.
  explicit CertificateViewerDialog(net::X509Certificate* cert);

  // Overridden from HtmlDialogUI::Delegate:
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      TabContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

  // The certificate being viewed.
  scoped_refptr<net::X509Certificate> cert_;

  // The argument string for the dialog which passes the certificate pointer.
  std::string json_args_;

  // The title of the certificate viewer dialog, Certificate Viewer: CN.
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerDialog);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_H_
