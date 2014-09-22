// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mixin_based_browser_test.h"

namespace chromeos {

MixinBasedBrowserTest::MixinBasedBrowserTest() : setup_was_launched_(false) {
}

MixinBasedBrowserTest::~MixinBasedBrowserTest() {
}

void MixinBasedBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  setup_was_launched_ = true;
  for (ScopedVector<Mixin>::iterator it = mixins_.begin(); it != mixins_.end();
       ++it) {
    (*it)->SetUpCommandLine(command_line);
  }
  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void MixinBasedBrowserTest::SetUpInProcessBrowserTestFixture() {
  setup_was_launched_ = true;
  for (ScopedVector<Mixin>::iterator it = mixins_.begin(); it != mixins_.end();
       ++it) {
    (*it)->SetUpInProcessBrowserTestFixture();
  }
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void MixinBasedBrowserTest::SetUpOnMainThread() {
  setup_was_launched_ = true;
  for (ScopedVector<Mixin>::iterator it = mixins_.begin(); it != mixins_.end();
       ++it) {
    (*it)->SetUpOnMainThread();
  }
  InProcessBrowserTest::SetUpOnMainThread();
}

void MixinBasedBrowserTest::TearDownOnMainThread() {
  InProcessBrowserTest::TearDownOnMainThread();
  for (ScopedVector<Mixin>::reverse_iterator it = mixins_.rbegin();
       it != mixins_.rend();
       ++it) {
    (*it)->TearDownInProcessBrowserTestFixture();
  }
}
void MixinBasedBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  for (ScopedVector<Mixin>::reverse_iterator it = mixins_.rbegin();
       it != mixins_.rend();
       ++it) {
    (*it)->TearDownInProcessBrowserTestFixture();
  }
}

void MixinBasedBrowserTest::AddMixin(MixinBasedBrowserTest::Mixin* mixin) {
  CHECK(!setup_was_launched_)
      << "You are trying to add a mixin after setting up has already started.";
  mixins_.push_back(mixin);
}

}  // namespace chromeos
