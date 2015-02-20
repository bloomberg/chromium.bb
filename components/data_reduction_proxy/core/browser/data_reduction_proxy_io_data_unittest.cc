// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/net_log.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used only to verify that a wrapped network delegate gets called.
class CountingNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  CountingNetworkDelegate() : created_requests_(0) {
  }

  ~CountingNetworkDelegate() final {
  }

  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) final {
    created_requests_++;
    return net::OK;
  }

  int created_requests() const {
    return created_requests_;
  }

 private:
  int created_requests_;
};

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyIODataTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterSimpleProfilePrefs(prefs_.registry());
  }

  void RequestCallback(int err) {
  }


  scoped_refptr<base::MessageLoopProxy> message_loop_proxy() {
    return loop_.message_loop_proxy();
  }

  net::TestDelegate* delegate() {
    return &delegate_;
  }

  const net::TestURLRequestContext& context() const {
    return context_;
  }

  net::NetLog* net_log() {
    return &net_log_;
  }

  PrefService* prefs() {
    return &prefs_;
  }

 private:
  base::MessageLoopForIO loop_;
  net::TestDelegate delegate_;
  net::TestURLRequestContext context_;
  net::NetLog net_log_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(DataReductionProxyIODataTest, TestConstruction) {
  scoped_ptr<DataReductionProxyIOData> io_data(new DataReductionProxyIOData(
      Client::UNKNOWN, DataReductionProxyParams::kAllowed, net_log(),
      message_loop_proxy(), message_loop_proxy(), false /* enable_quic */));

  // Check that io_data creates an interceptor. Such an interceptor is
  // thoroughly tested by DataReductionProxyInterceptoTest.
  scoped_ptr<net::URLRequestInterceptor> interceptor =
      io_data->CreateInterceptor();
  EXPECT_NE(nullptr, interceptor.get());

  // When creating a network delegate, expect that it properly wraps a
  // network delegate. Such a network delegate is thoroughly tested by
  // DataReductionProxyNetworkDelegateTest.
   scoped_ptr<net::URLRequest> fake_request =
       context().CreateRequest(
           GURL("http://www.foo.com/"), net::IDLE, delegate(), NULL);
  CountingNetworkDelegate* wrapped_network_delegate =
      new CountingNetworkDelegate();
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate =
      io_data->CreateNetworkDelegate(
          make_scoped_ptr(wrapped_network_delegate), false);
  network_delegate->NotifyBeforeURLRequest(
      fake_request.get(),
      base::Bind(&DataReductionProxyIODataTest::RequestCallback,
                 base::Unretained(this)), nullptr);
  EXPECT_EQ(1, wrapped_network_delegate->created_requests());
  EXPECT_EQ(nullptr, io_data->usage_stats());

  // Creating a second delegate with bypass statistics tracking should result
  // in usage stats being created.
  io_data->CreateNetworkDelegate(make_scoped_ptr(new CountingNetworkDelegate()),
                                 true);
  EXPECT_NE(nullptr, io_data->usage_stats());

  // The Data Reduction Proxy isn't actually enabled here.
  io_data->InitOnUIThread(prefs());
  EXPECT_FALSE(io_data->IsEnabled());
  io_data->ShutdownOnUIThread();
}

}  // namespace data_reduction_proxy
