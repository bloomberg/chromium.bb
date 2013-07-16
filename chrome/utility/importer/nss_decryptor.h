// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_H_
#define CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_H_

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "chrome/utility/importer/nss_decryptor_mac.h"
#elif defined(OS_WIN)
#include "chrome/utility/importer/nss_decryptor_win.h"
#elif defined(USE_OPENSSL)
// TODO(joth): It should be an error to include this file with USE_OPENSSL
// defined. (Unless there is a way to do nss decrypt with OpenSSL). Ideally
// we remove the importers that depend on NSS when doing USE_OPENSSL builds, but
// that is going to take some non-trivial refactoring so in the meantime we're
// just falling back to a no-op implementation.
#include "chrome/utility/importer/nss_decryptor_null.h"
#elif defined(USE_NSS)
#include "chrome/utility/importer/nss_decryptor_system_nss.h"
#endif

#endif  // CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_H_
