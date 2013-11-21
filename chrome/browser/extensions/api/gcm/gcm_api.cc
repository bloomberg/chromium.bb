// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcm/gcm_api.h"

namespace extensions {

bool GcmRegisterFunction::RunImpl() {
  return true;
}

bool GcmSendFunction::RunImpl() {
  return true;
}

}  // namespace extensions
