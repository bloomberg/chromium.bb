// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CERTIFICATE_DIALOGS_H_
#define CHROME_BROWSER_UI_GTK_CERTIFICATE_DIALOGS_H_
#pragma once

#include "chrome/browser/ui/shell_dialogs.h"
#include "net/base/x509_certificate.h"

void ShowCertSelectFileDialog(SelectFileDialog* select_file_dialog,
                              SelectFileDialog::Type type,
                              const FilePath& suggested_path,
                              gfx::NativeWindow parent,
                              void* params);

void ShowCertExportDialog(gfx::NativeWindow parent,
                          net::X509Certificate::OSCertHandle cert);

#endif  // CHROME_BROWSER_UI_GTK_CERTIFICATE_DIALOGS_H_
