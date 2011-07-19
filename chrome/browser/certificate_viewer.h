// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_VIEWER_H_
#define CHROME_BROWSER_CERTIFICATE_VIEWER_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace net {

class X509Certificate;

}  // namespace net

// Opens a certificate viewer under |parent| to display the certificate from
// the |CertStore| with id |cert_id|.
void ShowCertificateViewerByID(gfx::NativeWindow parent, int cert_id);

// Opens a certificate viewer under |parent| to display |cert|.
void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate* cert);

#endif  // CHROME_BROWSER_CERTIFICATE_VIEWER_H_
