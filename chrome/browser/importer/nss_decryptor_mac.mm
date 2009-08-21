// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/browser/importer/nss_decryptor_mac.h"

#include "base/logging.h"

// static
const wchar_t NSSDecryptor::kNSS3Library[] = L"libnss3.dylib";
const wchar_t NSSDecryptor::kSoftokn3Library[] = L"libsoftokn3.dylib";
const wchar_t NSSDecryptor::kPLDS4Library[] = L"libplds4.dylib";
const wchar_t NSSDecryptor::kNSPR4Library[] = L"libnspr4.dylib";

bool NSSDecryptor::Init(const std::wstring& dll_path,
                        const std::wstring& db_path) {
  // TODO(port): Load the NSS libraries and call InitNSS()
  // http://code.google.com/p/chromium/issues/detail?id=15455
  NOTIMPLEMENTED();
  return false;
}
