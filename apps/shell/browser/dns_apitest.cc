// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/test/shell_test.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/dns/dns_api.h"
#include "extensions/browser/api/dns/host_resolver_wrapper.h"
#include "extensions/browser/api/dns/mock_host_resolver_creator.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "net/base/net_errors.h"

using extensions::ExtensionFunctionDispatcher;
using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

namespace {

class TestFunctionDispatcherDelegate
    : public ExtensionFunctionDispatcher::Delegate {
 public:
  TestFunctionDispatcherDelegate() {}
  virtual ~TestFunctionDispatcherDelegate() {}

  // NULL implementation.
 private:
  DISALLOW_COPY_AND_ASSIGN(TestFunctionDispatcherDelegate);
};

}  // namespace

class DnsApiTest : public apps::AppShellTest {
 public:
  DnsApiTest() : resolver_creator_(new extensions::MockHostResolverCreator()) {}

 private:
  virtual void SetUpOnMainThread() OVERRIDE {
    apps::AppShellTest::SetUpOnMainThread();
    extensions::HostResolverWrapper::GetInstance()->SetHostResolverForTesting(
        resolver_creator_->CreateMockHostResolver());
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    extensions::HostResolverWrapper::GetInstance()->SetHostResolverForTesting(
        NULL);
    resolver_creator_->DeleteMockHostResolver();
    apps::AppShellTest::TearDownOnMainThread();
  }

  // The MockHostResolver asserts that it's used on the same thread on which
  // it's created, which is actually a stronger rule than its real counterpart.
  // But that's fine; it's good practice.
  scoped_refptr<extensions::MockHostResolverCreator> resolver_creator_;
};

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveIPLiteral) {
  scoped_refptr<extensions::DnsResolveFunction> resolve_function(
      new extensions::DnsResolveFunction());
  scoped_refptr<extensions::Extension> empty_extension(
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder().Set("name", "Test").Set(
              "version", "1.0"))
          .Build());

  resolve_function->set_extension(empty_extension.get());
  resolve_function->set_has_callback(true);

  TestFunctionDispatcherDelegate delegate;
  scoped_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(browser_context(), &delegate));

  scoped_ptr<base::Value> result(
      RunFunctionAndReturnSingleResult(resolve_function.get(),
                                       "[\"127.0.0.1\"]",
                                       browser_context(),
                                       dispatcher.Pass()));
  base::DictionaryValue* dict = NULL;
  ASSERT_TRUE(result->GetAsDictionary(&dict));

  int result_code = 0;
  EXPECT_TRUE(dict->GetInteger("resultCode", &result_code));
  EXPECT_EQ(net::OK, result_code);

  std::string address;
  EXPECT_TRUE(dict->GetString("address", &address));
  EXPECT_EQ("127.0.0.1", address);
}

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveHostname) {
  scoped_refptr<extensions::DnsResolveFunction> resolve_function(
      new extensions::DnsResolveFunction());
  scoped_refptr<extensions::Extension> empty_extension(
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder().Set("name", "Test").Set(
              "version", "1.0"))
          .Build());

  resolve_function->set_extension(empty_extension.get());
  resolve_function->set_has_callback(true);

  TestFunctionDispatcherDelegate delegate;
  scoped_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(browser_context(), &delegate));

  std::string function_arguments("[\"");
  function_arguments += extensions::MockHostResolverCreator::kHostname;
  function_arguments += "\"]";
  scoped_ptr<base::Value> result(
      RunFunctionAndReturnSingleResult(resolve_function.get(),
                                       function_arguments,
                                       browser_context(),
                                       dispatcher.Pass()));
  base::DictionaryValue* dict = NULL;
  ASSERT_TRUE(result->GetAsDictionary(&dict));

  int result_code = 0;
  EXPECT_TRUE(dict->GetInteger("resultCode", &result_code));
  EXPECT_EQ(net::OK, result_code);

  std::string address;
  EXPECT_TRUE(dict->GetString("address", &address));
  EXPECT_EQ(extensions::MockHostResolverCreator::kAddress, address);
}
