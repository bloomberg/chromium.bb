// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/api/dns/dns_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_resolver.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using extension_function_test_utils::CreateEmptyExtension;
using extension_function_test_utils::RunFunctionAndReturnResult;

namespace {

class DnsApiTest : public PlatformAppApiTest {
 public:
  static const std::string kHostname;
  static const std::string kAddress;

  DnsApiTest() : resolver_event_(true, false),
                 mock_host_resolver_(NULL) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    CreateMockHostResolverOnIOThread();
    extensions::DnsResolveFunction::set_host_resolver_for_testing(
        get_mock_host_resolver());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    if (mock_host_resolver_) {
      extensions::DnsResolveFunction::set_host_resolver_for_testing(NULL);
      DeleteMockHostResolverOnIOThread();
    }
  }

  void CreateMockHostResolverOnIOThread() {
    bool result = BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DnsApiTest::FinishMockHostResolverCreation, this));
    DCHECK(result);

    base::TimeDelta max_time = base::TimeDelta::FromSeconds(5);
    ASSERT_TRUE(resolver_event_.TimedWait(max_time));

  }

  void FinishMockHostResolverCreation() {
    mock_host_resolver_ = new net::MockHostResolver();
    get_mock_host_resolver()->rules()->AddRule(kHostname, kAddress);
    get_mock_host_resolver()->rules()->AddSimulatedFailure(
        "this.hostname.is.bogus");
    resolver_event_.Signal();
  }

  void DeleteMockHostResolverOnIOThread() {
    if (!mock_host_resolver_)
      return;
    resolver_event_.Reset();
    bool result = BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DnsApiTest::FinishMockHostResolverDeletion, this));
    DCHECK(result);

    base::TimeDelta max_time = base::TimeDelta::FromSeconds(5);
    ASSERT_TRUE(resolver_event_.TimedWait(max_time));
  }

  void FinishMockHostResolverDeletion() {
    delete(mock_host_resolver_);
    resolver_event_.Signal();
  }

  net::MockHostResolver* get_mock_host_resolver() {
    return mock_host_resolver_;
  }

 private:
  base::WaitableEvent resolver_event_;

  // The MockHostResolver asserts that it's used on the same thread on which
  // it's created, which is actually a stronger rule than its real counterpart.
  // But that's fine; it's good practice.
  //
  // Plain pointer because we have to manage lifetime manually.
  net::MockHostResolver* mock_host_resolver_;
};
const std::string DnsApiTest::kHostname = "www.sowbug.org";
const std::string DnsApiTest::kAddress = "9.8.7.6";

}  // namespace

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveIPLiteral) {
  scoped_refptr<extensions::DnsResolveFunction> resolve_function(
      new extensions::DnsResolveFunction());
  scoped_refptr<extensions::Extension> empty_extension(CreateEmptyExtension());

  resolve_function->set_extension(empty_extension.get());
  resolve_function->set_has_callback(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      resolve_function, "[\"127.0.0.1\"]", browser()));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  DictionaryValue *value = static_cast<DictionaryValue*>(result.get());

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
  function_arguments += DnsApiTest::kHostname;
  function_arguments += "\"]";
  scoped_ptr<base::Value> result(
      RunFunctionAndReturnResult(resolve_function.get(),
                                 function_arguments, browser()));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  DictionaryValue *value = static_cast<DictionaryValue*>(result.get());

  int resultCode;
  EXPECT_TRUE(value->GetInteger("resultCode", &resultCode));
  EXPECT_EQ(net::OK, resultCode);

  std::string address;
  EXPECT_TRUE(value->GetString("address", &address));
  EXPECT_EQ(DnsApiTest::kAddress, address);
}

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsExtension) {
  ASSERT_TRUE(RunExtensionTest("dns/api")) << message_;
}
