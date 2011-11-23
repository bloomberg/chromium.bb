// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "net/base/x509_certificate.h"
#include "ui/gfx/native_widget_types.h"

// Displays the native or WebUI certificate viewer dialog for the given
// certificate.
void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate*);

// Dialog for displaying detailed certificate information. This is used in linux
// and chromeos builds to display detailed information in a floating dialog when
// the user clicks on "Certificate Information" from the lock icon of a web site
// or "View" from the Certificate Manager.
class CertificateViewerDialog : private HtmlDialogUIDelegate {
 public:
  // Shows the certificate viewer dialog for the passed in certificate.
  static void ShowDialog(gfx::NativeWindow parent,
                         net::X509Certificate* cert);
  virtual ~CertificateViewerDialog();

 private:
  // Construct a certificate viewer for the passed in certificate. A reference
  // to the certificate pointer is added for the lifetime of the certificate
  // viewer.
  explicit CertificateViewerDialog(net::X509Certificate* cert);

  // Show the dialog using the given parent window.
  void Show(gfx::NativeWindow parent);

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

  // The window displaying this dialog.
  gfx::NativeWindow window_;

  // The title of the certificate viewer dialog, Certificate Viewer: CN.
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerDialog);
};

// Dialog handler which handles calls from the JS WebUI code to view certificate
// details and export the certificate.
class CertificateViewerDialogHandler : public WebUIMessageHandler {
 public:
  CertificateViewerDialogHandler(gfx::NativeWindow window,
                                 net::X509Certificate* cert);
  virtual ~CertificateViewerDialogHandler();

  // Overridden from WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Brings up the export certificate dialog for the chosen certificate in the
  // chain.
  //
  // The input is an integer index to the certificate in the chain to export.
  void ExportCertificate(const base::ListValue* args);

  // Gets the details for a specific certificate in the certificate chain. Calls
  // the javascript function cert_viewer.getCertificateFields with a tree
  // structure containing the fields and values for certain nodes.
  //
  // The input is an integer index to the certificate in the chain to view.
  void RequestCertificateFields(const base::ListValue* args);

  // Extracts the certificate details and returns them to the javascript
  // function cert_viewer.getCertificateInfo in a dictionary structure.
  void RequestCertificateInfo(const base::ListValue* args);

  // The certificate being viewed.
  scoped_refptr<net::X509Certificate> cert_;

  // The dialog window.
  gfx::NativeWindow window_;

  // The certificate chain.
  net::X509Certificate::OSCertHandles cert_chain_;

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerDialogHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
