// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/mock_password_store.h"

MockPasswordStore::MockPasswordStore() {}

MockPasswordStore::~MockPasswordStore() {}

// static
ProfileKeyedBase* MockPasswordStore::Build(Profile* profile) {
  return new MockPasswordStore;
}

void MockPasswordStore::ShutdownOnUIThread() {}
