// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cert/x509_certificate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class WebContents;
}

namespace ui {
class WebDialogObserver;
}

// Dialog for displaying detailed certificate information. This is used in linux
// and chromeos builds to display detailed information in a floating dialog when
// the user clicks on "Certificate Information" from the lock icon of a web site
// or "View" from the Certificate Manager.
class CertificateViewerDialog : private ui::WebDialogDelegate {
 public:
  // Construct a certificate viewer for the passed in certificate. A reference
  // to the certificate pointer is added for the lifetime of the certificate
  // viewer.
  explicit CertificateViewerDialog(net::X509Certificate* cert);
  virtual ~CertificateViewerDialog();

  // Show the dialog using the given parent window.
  void Show(content::WebContents* web_contents, gfx::NativeWindow parent);

  // Add WebDialogObserver for this dialog.
  void AddObserver(ui::WebDialogObserver* observer);

  // Remove WebDialogObserver for this dialog.
  void RemoveObserver(ui::WebDialogObserver* observer);

 private:
  // Overridden from ui::WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogShown(
      content::WebUI* webui,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  // The certificate being viewed.
  scoped_refptr<net::X509Certificate> cert_;

  // The window displaying this dialog.
  gfx::NativeWindow window_;

  // The title of the certificate viewer dialog, Certificate Viewer: CN.
  string16 title_;

  ObserverList<ui::WebDialogObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerDialog);
};

// Dialog handler which handles calls from the JS WebUI code to view certificate
// details and export the certificate.
class CertificateViewerDialogHandler : public content::WebUIMessageHandler {
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

  // Helper function to get the certificate index from |args|. Returns -1 if
  // the index is out of range.
  int GetCertificateIndex(const base::ListValue* args) const;

  // The certificate being viewed.
  scoped_refptr<net::X509Certificate> cert_;

  // The dialog window.
  gfx::NativeWindow window_;

  // The certificate chain.
  net::X509Certificate::OSCertHandles cert_chain_;

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerDialogHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
