// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_H_
#define CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_H_

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "chrome/browser/importer/nss_decryptor_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/importer/nss_decryptor_win.h"
#elif defined(USE_NSS)
#include "chrome/browser/importer/nss_decryptor_system_nss.h"
#endif

#endif  // CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_H_
