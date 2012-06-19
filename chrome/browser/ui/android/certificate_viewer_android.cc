// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include "base/logging.h"
#include "net/base/x509_certificate.h"

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // TODO(yfriedman, bulach): Hook this up to Java ui code: crbug.com/114822
  NOTIMPLEMENTED();
}
