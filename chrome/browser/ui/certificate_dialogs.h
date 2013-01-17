// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CERTIFICATE_DIALOGS_H_
#define CHROME_BROWSER_UI_CERTIFICATE_DIALOGS_H_

#include "net/base/x509_certificate.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

void ShowCertSelectFileDialog(ui::SelectFileDialog* select_file_dialog,
                              ui::SelectFileDialog::Type type,
                              const FilePath& suggested_path,
                              gfx::NativeWindow parent,
                              void* params);

void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          net::X509Certificate::OSCertHandle cert);

#endif  // CHROME_BROWSER_UI_CERTIFICATE_DIALOGS_H_
