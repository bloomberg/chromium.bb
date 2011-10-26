// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/keys.h"

namespace protector {

// When changing the key, be sure to add migration code to keep user's
// settings safe.
const char kProtectorSigningKey[] = "Please, don't change this Chrome setting";

}  // namespace protector
