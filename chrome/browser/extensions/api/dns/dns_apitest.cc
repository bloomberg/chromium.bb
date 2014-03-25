// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/dns/mock_host_resolver_creator.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "extensions/browser/api/dns/dns_api.h"
#include "extensions/browser/api/dns/host_resolver_wrapper.h"
#include "extensions/common/switches.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/mock_host_resolver.h"

using extension_function_test_utils::CreateEmptyExtension;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace {

class DnsApiTest : public ExtensionApiTest {
 public:
  DnsApiTest() : resolver_event_(true, false),
                 resolver_creator_(new extensions::MockHostResolverCreator()) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    extensions::HostResolverWrapper::GetInstance()->SetHostResolverForTesting(
        resolver_creator_->CreateMockHostResolver());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    extensions::HostResolverWrapper::GetInstance()->
        SetHostResolverForTesting(NULL);
    resolver_creator_->DeleteMockHostResolver();
  }

 private:
  base::WaitableEvent resolver_event_;

  // The MockHostResolver asserts that it's used on the same thread on which
  // it's created, which is actually a stronger rule than its real counterpart.
  // But that's fine; it's good practice.
  scoped_refptr<extensions::MockHostResolverCreator> resolver_creator_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveIPLiteral) {
  scoped_refptr<extensions::DnsResolveFunction> resolve_function(
      new extensions::DnsResolveFunction());
  scoped_refptr<extensions::Extension> empty_extension(CreateEmptyExtension());

  resolve_function->set_extension(empty_extension.get());
  resolve_function->set_has_callback(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
      resolve_function.get(), "[\"127.0.0.1\"]", browser()));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  base::DictionaryValue *value =
      static_cast<base::DictionaryValue*>(result.get());

  int resultCode;
  EXPECT_TRUE(value->GetInteger("resultCode", &resultCode));
  EXPECT_EQ(net::OK, resultCode);

  std::string address;
  EXPECT_TRUE(value->GetString("address", &address));
  EXPECT_EQ("127.0.0.1", address);
}

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveHostname) {
  scoped_refptr<extensions::DnsResolveFunction> resolve_function(
      new extensions::DnsResolveFunction());
  scoped_refptr<extensions::Extension> empty_extension(CreateEmptyExtension());

  resolve_function->set_extension(empty_extension.get());
  resolve_function->set_has_callback(true);

  std::string function_arguments("[\"");
  function_arguments += extensions::MockHostResolverCreator::kHostname;
  function_arguments += "\"]";
  scoped_ptr<base::Value> result(
      RunFunctionAndReturnSingleResult(resolve_function.get(),
                                       function_arguments, browser()));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  base::DictionaryValue *value =
      static_cast<base::DictionaryValue*>(result.get());

  int resultCode;
  EXPECT_TRUE(value->GetInteger("resultCode", &resultCode));
  EXPECT_EQ(net::OK, resultCode);

  std::string address;
  EXPECT_TRUE(value->GetString("address", &address));
  EXPECT_EQ(extensions::MockHostResolverCreator::kAddress, address);
}

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsExtension) {
  ASSERT_TRUE(RunExtensionTest("dns/api")) << message_;
}
