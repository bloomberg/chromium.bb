// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CERTIFICATE_DIALOGS_H_
#define CHROME_BROWSER_GTK_CERTIFICATE_DIALOGS_H_
#pragma once

#include "chrome/browser/shell_dialogs.h"
#include "net/base/x509_certificate.h"

void ShowCertExportDialog(gfx::NativeWindow parent,
                          net::X509Certificate::OSCertHandle cert);

#endif  // CHROME_BROWSER_GTK_CERTIFICATE_DIALOGS_H_
