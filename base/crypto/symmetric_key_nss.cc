// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/symmetric_key.h"

#include <nss.h>
#include <pk11pub.h>

#include "base/logging.h"

namespace base {

bool SymmetricKey::GetRawKey(std::string* raw_key) {
  SECStatus rv = PK11_ExtractKeyValue(key_.get());
  if (SECSuccess != rv)
    return false;

  SECItem* key_item = PK11_GetKeyData(key_.get());
  if (!key_item)
    return false;

  raw_key->assign(reinterpret_cast<char*>(key_item->data), key_item->len);
  return true;
}

}  // namespace base
