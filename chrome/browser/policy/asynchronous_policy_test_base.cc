// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_test_base.h"

namespace policy {

ProviderDelegateMock::ProviderDelegateMock()
    : AsynchronousPolicyProvider::Delegate() {}

ProviderDelegateMock::~ProviderDelegateMock() {}

AsynchronousPolicyTestBase::AsynchronousPolicyTestBase()
    : ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE, &loop_) {}

AsynchronousPolicyTestBase::~AsynchronousPolicyTestBase() {}

void AsynchronousPolicyTestBase::SetUp() {
  delegate_.reset(new ProviderDelegateMock());
}

void AsynchronousPolicyTestBase::TearDown() {
  loop_.RunAllPending();
}

}  // namespace policy
