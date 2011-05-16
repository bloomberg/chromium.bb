// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"

namespace chromeos {

MockKeyUtils::MockKeyUtils() {}

MockKeyUtils::~MockKeyUtils() {}

MockInjector::MockInjector(MockKeyUtils* mock) : transient_(mock) {}

MockInjector::~MockInjector() {}

OwnerKeyUtils* MockInjector::CreateOwnerKeyUtils() {
  return transient_.get();
}
}  // namespace chromeos
