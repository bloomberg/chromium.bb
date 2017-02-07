// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_INFO_UTIL_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_INFO_UTIL_WIN_H_

#include "chrome/browser/conflicts/module_database_win.h"

// Extracts information about the certificate of the given |file|, populating
// |cert_info|. It is expected that |cert_info| be freshly constructed.
void GetCertificateInfo(const base::FilePath& file,
                        ModuleDatabase::CertificateInfo* cert_info);

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_INFO_UTIL_WIN_H_
