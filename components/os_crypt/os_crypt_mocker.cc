// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt_mocker.h"

#include "components/os_crypt/os_crypt.h"

// static
void OSCryptMocker::SetUpWithSingleton() {
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(true);
#endif
}

// static
void OSCryptMocker::TearDown() {
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(false);
#endif
}
