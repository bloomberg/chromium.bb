// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/dns/host_resolver_wrapper.h"
#include "extensions/browser/api/dns/mock_host_resolver_creator.h"
#include "net/dns/mock_host_resolver.h"

class DnsApiTest : public ExtensionApiTest {
 public:
  DnsApiTest() : resolver_creator_(new extensions::MockHostResolverCreator()) {}

 private:
  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    extensions::HostResolverWrapper::GetInstance()->SetHostResolverForTesting(
        resolver_creator_->CreateMockHostResolver());
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    extensions::HostResolverWrapper::GetInstance()->
        SetHostResolverForTesting(NULL);
    resolver_creator_->DeleteMockHostResolver();
    ExtensionApiTest::TearDownOnMainThread();
  }

  // The MockHostResolver asserts that it's used on the same thread on which
  // it's created, which is actually a stronger rule than its real counterpart.
  // But that's fine; it's good practice.
  scoped_refptr<extensions::MockHostResolverCreator> resolver_creator_;
};

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsExtension) {
  ASSERT_TRUE(RunExtensionTest("dns/api")) << message_;
}
