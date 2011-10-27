// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_util.h"

#include <windows.h>
#include <wincrypt.h>
#include "base/file_path.h"

namespace safe_browsing {
namespace signature_util {

bool IsSigned(const FilePath& file_path) {
  // Get message handle and store handle from the signed file.
  BOOL result = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                 file_path.value().c_str(),
                                 CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                 CERT_QUERY_FORMAT_FLAG_BINARY,
                                 0,      // flags
                                 NULL,   // encoding
                                 NULL,   // content type
                                 NULL,   // format type
                                 NULL,   // HCERTSTORE
                                 NULL,   // HCRYPTMSG
                                 NULL);  // context
  return result == TRUE;
}

}  // namespace signature_util
}  // namespace safe_browsing
