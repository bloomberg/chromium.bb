// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/nss_decryptor_linux.h"

#include "base/nss_init.h"

NSSDecryptor::NSSDecryptor() : is_nss_initialized_(false) {}
NSSDecryptor::~NSSDecryptor() {}

bool NSSDecryptor::Init(const std::wstring& /* dll_path */,
                        const std::wstring& /* db_path */) {
  base::EnsureNSSInit();
  is_nss_initialized_ = true;
  return true;
}
