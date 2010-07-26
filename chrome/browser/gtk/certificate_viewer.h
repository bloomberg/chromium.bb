// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CERTIFICATE_VIEWER_H_
#define CHROME_BROWSER_GTK_CERTIFICATE_VIEWER_H_
#pragma once

#include "chrome/browser/certificate_viewer.h"
#include "gfx/native_widget_types.h"

typedef struct CERTCertificateStr CERTCertificate;

void ShowCertificateViewer(gfx::NativeWindow parent, CERTCertificate* cert);

#endif  // CHROME_BROWSER_GTK_CERTIFICATE_VIEWER_H_
