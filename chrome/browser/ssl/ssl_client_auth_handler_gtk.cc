// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "base/logging.h"
#include "net/base/x509_certificate.h"

void SSLClientAuthHandler::DoSelectCertificate() {
  NOTIMPLEMENTED();
  CertificateSelected(NULL);
}
