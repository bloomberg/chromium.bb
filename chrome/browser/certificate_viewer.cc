// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include "content/public/browser/cert_store.h"
#include "net/cert/x509_certificate.h"

void ShowCertificateViewerByID(content::WebContents* web_contents,
                               gfx::NativeWindow parent,
                               int cert_id) {
  scoped_refptr<net::X509Certificate> cert;
  content::CertStore::GetInstance()->RetrieveCert(cert_id, &cert);
  if (!cert.get()) {
    // The certificate was not found. Could be that the renderer crashed before
    // we displayed the page info.
    return;
  }
  ShowCertificateViewer(web_contents, parent, cert.get());
}
