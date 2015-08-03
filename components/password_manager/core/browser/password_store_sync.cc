// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_sync.h"

namespace password_manager {

PasswordStoreSync::PasswordStoreSync() : is_alive_(true) {}

PasswordStoreSync::~PasswordStoreSync() {
  CHECK(is_alive_);
  is_alive_ = false;
}

}  // namespace password_manager
