// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CERTIFICATE_VIEWER_H_
#define CHROME_BROWSER_UI_GTK_CERTIFICATE_VIEWER_H_
#pragma once

#include "content/browser/certificate_viewer.h"
#include "net/base/x509_certificate.h"
#include "ui/gfx/native_widget_types.h"

void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate::OSCertHandle);

#endif  // CHROME_BROWSER_UI_GTK_CERTIFICATE_VIEWER_H_
