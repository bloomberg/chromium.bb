// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_password_store.h"

namespace password_manager {

MockPasswordStore::MockPasswordStore()
    : PasswordStore(
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current()) {
}

MockPasswordStore::~MockPasswordStore() {}

}  // namespace password_manager
