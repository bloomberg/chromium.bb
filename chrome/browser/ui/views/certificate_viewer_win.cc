// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include <windows.h>
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/host_desktop.h"
#include "net/cert/x509_certificate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace {

void ShowCertificateViewerImpl(content::WebContents* web_contents,
                               HWND parent,
                               net::X509Certificate* cert) {
  // Create a new cert context and store containing just the certificate
  // and its intermediate certificates.
  PCCERT_CONTEXT cert_list = cert->CreateOSCertChainForCert();
  CHECK(cert_list);

  CRYPTUI_VIEWCERTIFICATE_STRUCT view_info = { 0 };
  view_info.dwSize = sizeof(view_info);
  // We set our parent to the tab window. This makes the cert dialog created
  // in CryptUIDlgViewCertificate modal to the browser.
  view_info.hwndParent = parent;
  view_info.dwFlags = CRYPTUI_DISABLE_EDITPROPERTIES |
                      CRYPTUI_DISABLE_ADDTOSTORE;
  view_info.pCertContext = cert_list;
  HCERTSTORE cert_store = cert_list->hCertStore;
  view_info.cStores = 1;
  view_info.rghStores = &cert_store;
  BOOL properties_changed;

  // We must allow nested tasks to dispatch so that, e.g. gpu tasks are
  // processed for painting. This allows a second window to continue painting
  // while the the certificate dialog is open.
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  // This next call blocks but keeps processing windows messages, making it
  // modal to the browser window.
  BOOL rv = ::CryptUIDlgViewCertificate(&view_info, &properties_changed);

  CertFreeCertificateContext(cert_list);
}

}  // namespace

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  if (chrome::GetHostDesktopTypeForNativeWindow(parent) !=
      chrome::HOST_DESKTOP_TYPE_ASH) {
    ShowCertificateViewerImpl(
        web_contents,
        parent->GetHost()->GetAcceleratedWidget(), cert);
  } else {
    NOTIMPLEMENTED();
  }
}
