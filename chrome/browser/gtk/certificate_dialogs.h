// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CERTIFICATE_DIALOGS_H_
#define CHROME_BROWSER_GTK_CERTIFICATE_DIALOGS_H_

#include <cert.h>

#include "chrome/browser/shell_dialogs.h"

void ShowCertExportDialog(gfx::NativeWindow parent, CERTCertificate* cert);

#endif  // CHROME_BROWSER_GTK_CERTIFICATE_DIALOGS_H_
