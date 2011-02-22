// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/certificate_viewer.h"

#include "content/browser/cert_store.h"

void ShowCertificateViewerByID(gfx::NativeWindow parent, int cert_id) {
  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetInstance()->RetrieveCert(cert_id, &cert);
  if (!cert.get()) {
    // The certificate was not found. Could be that the renderer crashed before
    // we displayed the page info.
    return;
  }
  ShowCertificateViewer(parent, cert);
}
