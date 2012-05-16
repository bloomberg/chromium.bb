// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_TEST_BASE_H_
#define CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_TEST_BASE_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyBundle;

// A delegate for testing that can feed arbitrary information to the loader.
class ProviderDelegateMock : public AsynchronousPolicyProvider::Delegate {
 public:
  ProviderDelegateMock();
  virtual ~ProviderDelegateMock();

  // Load() returns a scoped_ptr<PolicyBundle> but it can't be mocked because
  // scoped_ptr is moveable but not copyable. This override forwards the
  // call to MockLoad() which returns a PolicyBundle*, and returns a copy
  // wrapped in a passed scoped_ptr.
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE {
    scoped_ptr<PolicyBundle> bundle;
    PolicyBundle* loaded = MockLoad();
    if (loaded) {
      bundle.reset(new PolicyBundle());
      bundle->CopyFrom(*loaded);
    }
    return bundle.Pass();
  }

  MOCK_METHOD0(MockLoad, PolicyBundle*());

 private:
  DISALLOW_COPY_AND_ASSIGN(ProviderDelegateMock);
};

class AsynchronousPolicyTestBase : public testing::Test {
 public:
  AsynchronousPolicyTestBase();
  virtual ~AsynchronousPolicyTestBase();

  // testing::Test:
  virtual void TearDown() OVERRIDE;

 protected:
  // Create an actual IO loop (needed by FilePathWatcher).
  MessageLoopForIO loop_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyTestBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_TEST_BASE_H_
