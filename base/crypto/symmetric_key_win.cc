// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/symmetric_key.h"

namespace base {

bool SymmetricKey::GetRawKey(std::string* raw_key) {
  // TODO(albertb): Implement on Windows.
  return false;
}

}  // namespace base
