// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/stale_host_resolver.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver_proc.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "net/android/network_change_notifier_factory_android.h"
#endif

namespace cronet {

namespace {

const char kHostname[] = "example.com";
const char kCacheAddress[] = "1.1.1.1";
const char kNetworkAddress[] = "2.2.2.2";
const char kUninitializedAddress[] = "3.3.3.3";
const int kCacheEntryTTLSec = 300;

const int kNoStaleDelaySec = 0;
const int kLongStaleDelaySec = 3600;
const uint16_t kPort = 12345;

const int kAgeFreshSec = 0;
const int kAgeExpiredSec = kCacheEntryTTLSec * 2;

// How long to wait for resolve calls to return. If the tests are working
// correctly, we won't end up waiting this long -- it's just a backup.
const int kWaitTimeoutSec = 1;

net::AddressList MakeAddressList(const char* ip_address_str) {
  net::IPAddress address;
  bool rv = address.AssignFromIPLiteral(ip_address_str);
  DCHECK(rv);

  net::AddressList address_list;
  address_list.push_back(net::IPEndPoint(address, 0u));
  return address_list;
}

class MockHostResolverProc : public net::HostResolverProc {
 public:
  MockHostResolverProc() : HostResolverProc(nullptr) {}

  int Resolve(const std::string& hostname,
              net::AddressFamily address_family,
              net::HostResolverFlags host_resolver_flags,
              net::AddressList* address_list,
              int* os_error) override {
    *address_list = MakeAddressList(kNetworkAddress);
    return net::OK;
  }

 protected:
  ~MockHostResolverProc() override {}
};

class StaleHostResolverTest : public testing::Test {
 protected:
  StaleHostResolverTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        mock_proc_(new MockHostResolverProc()),
        resolver_(nullptr),
        resolve_pending_(false),
        resolve_complete_(false) {}

  ~StaleHostResolverTest() override {}

  void SetStaleDelay(int stale_delay_sec) {
    DCHECK(!resolver_);

    options_.delay = base::TimeDelta::FromSeconds(stale_delay_sec);
  }

  void SetStaleUsability(int max_expired_time_sec,
                         int max_stale_uses,
                         bool allow_other_network) {
    DCHECK(!resolver_);

    options_.max_expired_time =
        base::TimeDelta::FromSeconds(max_expired_time_sec);
    options_.max_stale_uses = max_stale_uses;
    options_.allow_other_network = allow_other_network;
  }

  void CreateResolver() {
    DCHECK(!resolver_);

    std::unique_ptr<net::HostResolverImpl> inner_resolver(
        net::HostResolver::CreateDefaultResolverImpl(nullptr));

    net::HostResolverImpl::ProcTaskParams proc_params(mock_proc_.get(), 1u);
    inner_resolver->set_proc_params_for_test(proc_params);

    stale_resolver_ = base::WrapUnique(
        new StaleHostResolver(std::move(inner_resolver), options_));
    resolver_ = stale_resolver_.get();
  }

  void DestroyResolver() {
    DCHECK(stale_resolver_);

    stale_resolver_.reset();
    resolver_ = nullptr;
  }

  void SetResolver(net::HostResolver* resolver) {
    DCHECK(!resolver_);

    resolver_ = resolver;
  }

  void ClearResolver() {
    DCHECK(resolver_);
    DCHECK(!stale_resolver_);

    resolver_ = nullptr;
  }

  void CreateNetworkChangeNotifier() {
#if defined(OS_ANDROID)
    net::NetworkChangeNotifier::SetFactory(
        new net::NetworkChangeNotifierFactoryAndroid());
#endif
    net::NetworkChangeNotifier::Create();
  }

  // Creates a cache entry for |kHostname| that is |age_sec| seconds old.
  void CreateCacheEntry(int age_sec, int error) {
    DCHECK(resolver_);
    DCHECK(resolver_->GetHostCache());

    base::TimeDelta ttl(base::TimeDelta::FromSeconds(kCacheEntryTTLSec));
    net::HostCache::Key key(kHostname, net::ADDRESS_FAMILY_IPV4, 0);
    net::HostCache::Entry entry(
        error,
        error == net::OK ? MakeAddressList(kCacheAddress) : net::AddressList(),
        ttl);
    base::TimeDelta age = base::TimeDelta::FromSeconds(age_sec);
    base::TimeTicks then = base::TimeTicks::Now() - age;
    resolver_->GetHostCache()->Set(key, entry, then, ttl);
  }

  void OnNetworkChange() {
    // Real network changes on Android will send both notifications.
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    net::NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
    base::RunLoop().RunUntilIdle();  // Wait for notification.
  }

  void LookupStale() {
    DCHECK(resolver_);
    DCHECK(resolver_->GetHostCache());

    net::HostCache::Key key(kHostname, net::ADDRESS_FAMILY_IPV4, 0);
    base::TimeTicks now = base::TimeTicks::Now();
    const net::HostCache::Entry* entry;
    net::HostCache::EntryStaleness stale;
    entry = resolver_->GetHostCache()->LookupStale(key, now, &stale);
    EXPECT_TRUE(entry);
    EXPECT_TRUE(stale.is_stale());
  }

  void Resolve() {
    DCHECK(resolver_);
    EXPECT_FALSE(resolve_pending_);

    net::HostResolver::RequestInfo info(net::HostPortPair(kHostname, kPort));
    info.set_address_family(net::ADDRESS_FAMILY_IPV4);

    resolve_pending_ = true;
    resolve_complete_ = false;
    resolve_addresses_ = MakeAddressList(kUninitializedAddress);
    resolve_error_ = net::ERR_UNEXPECTED;

    int rv =
        resolver_->Resolve(info, net::DEFAULT_PRIORITY, &resolve_addresses_,
                           base::Bind(&StaleHostResolverTest::OnResolveComplete,
                                      base::Unretained(this)),
                           &request_, net::NetLogWithSource());
    if (rv != net::ERR_IO_PENDING) {
      resolve_pending_ = false;
      resolve_complete_ = true;
      resolve_error_ = rv;
    }
  }

  void WaitForResolve() {
    if (!resolve_pending_)
      return;

    base::RunLoop run_loop;

    // Run until resolve completes or timeout.
    resolve_closure_ = run_loop.QuitWhenIdleClosure();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(),
        base::TimeDelta::FromSeconds(kWaitTimeoutSec));
    run_loop.Run();
  }

  void WaitForIdle() {
    base::RunLoop run_loop;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  void Cancel() {
    DCHECK(resolver_);
    EXPECT_TRUE(resolve_pending_);

    delete request_.release();

    resolve_pending_ = false;
  }

  void OnResolveComplete(int error) {
    EXPECT_TRUE(resolve_pending_);

    request_.reset();

    resolve_error_ = error;
    resolve_pending_ = false;
    resolve_complete_ = true;

    if (!resolve_closure_.is_null())
      base::ResetAndReturn(&resolve_closure_).Run();
  }

  bool resolve_complete() const { return resolve_complete_; }
  int resolve_error() const { return resolve_error_; }
  const net::AddressList& resolve_addresses() const {
    return resolve_addresses_;
  }

 private:
  // Needed for HostResolver to run HostResolverProc callbacks.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<MockHostResolverProc> mock_proc_;

  net::HostResolver* resolver_;
  StaleHostResolver::StaleOptions options_;
  std::unique_ptr<StaleHostResolver> stale_resolver_;

  base::TimeTicks now_;
  std::unique_ptr<net::HostResolver::Request> request_;
  bool resolve_pending_;
  bool resolve_complete_;
  net::AddressList resolve_addresses_;
  int resolve_error_;

  base::Closure resolve_closure_;
};

// Make sure that test harness can be created and destroyed without crashing.
TEST_F(StaleHostResolverTest, Null) {}

// Make sure that resolver can be created and destroyed without crashing.
TEST_F(StaleHostResolverTest, Create) {
  CreateResolver();
}

TEST_F(StaleHostResolverTest, Network) {
  CreateResolver();

  Resolve();
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(net::OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kNetworkAddress, resolve_addresses()[0].ToStringWithoutPort());
}

TEST_F(StaleHostResolverTest, FreshCache) {
  CreateResolver();
  CreateCacheEntry(kAgeFreshSec, net::OK);

  Resolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(net::OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());

  WaitForIdle();
}

TEST_F(StaleHostResolverTest, StaleCache) {
  SetStaleDelay(kNoStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, net::OK);

  Resolve();
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(net::OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());
}

TEST_F(StaleHostResolverTest, NetworkWithStaleCache) {
  SetStaleDelay(kLongStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, net::OK);

  Resolve();
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(net::OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kNetworkAddress, resolve_addresses()[0].ToStringWithoutPort());
}

TEST_F(StaleHostResolverTest, CancelWithNoCache) {
  SetStaleDelay(kNoStaleDelaySec);
  CreateResolver();

  Resolve();

  Cancel();

  EXPECT_FALSE(resolve_complete());

  // Make sure there's no lingering |OnResolveComplete()| callback waiting.
  WaitForIdle();
}

TEST_F(StaleHostResolverTest, CancelWithStaleCache) {
  SetStaleDelay(kLongStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, net::OK);

  Resolve();

  Cancel();

  EXPECT_FALSE(resolve_complete());

  // Make sure there's no lingering |OnResolveComplete()| callback waiting.
  WaitForIdle();
}

// CancelWithFreshCache makes no sense; the request would've returned
// synchronously.

TEST_F(StaleHostResolverTest, StaleUsability) {
  const struct {
    int max_expired_time_sec;
    int max_stale_uses;
    bool allow_other_network;

    int age_sec;
    int stale_use;
    int network_changes;
    int error;

    bool usable;
  } kUsabilityTestCases[] = {
      // Fresh data always accepted.
      {0, 0, true, -1, 1, 0, net::OK, true},
      {1, 1, false, -1, 1, 0, net::OK, true},

      // Unlimited expired time accepts non-zero time.
      {0, 0, true, 1, 1, 0, net::OK, true},

      // Limited expired time accepts before but not after limit.
      {2, 0, true, 1, 1, 0, net::OK, true},
      {2, 0, true, 3, 1, 0, net::OK, false},

      // Unlimited stale uses accepts first and later uses.
      {2, 0, true, 1, 1, 0, net::OK, true},
      {2, 0, true, 1, 9, 0, net::OK, true},

      // Limited stale uses accepts up to and including limit.
      {2, 2, true, 1, 1, 0, net::OK, true},
      {2, 2, true, 1, 2, 0, net::OK, true},
      {2, 2, true, 1, 3, 0, net::OK, false},
      {2, 2, true, 1, 9, 0, net::OK, false},

      // Allowing other networks accepts zero or more network changes.
      {2, 0, true, 1, 1, 0, net::OK, true},
      {2, 0, true, 1, 1, 1, net::OK, true},
      {2, 0, true, 1, 1, 9, net::OK, true},

      // Disallowing other networks only accepts zero network changes.
      {2, 0, false, 1, 1, 0, net::OK, true},
      {2, 0, false, 1, 1, 1, net::OK, false},
      {2, 0, false, 1, 1, 9, net::OK, false},

      // Errors are only accepted if fresh.
      {0, 0, true, -1, 1, 0, net::ERR_NAME_NOT_RESOLVED, true},
      {1, 1, false, -1, 1, 0, net::ERR_NAME_NOT_RESOLVED, true},
      {0, 0, true, 1, 1, 0, net::ERR_NAME_NOT_RESOLVED, false},
      {2, 0, true, 1, 1, 0, net::ERR_NAME_NOT_RESOLVED, false},
      {2, 0, true, 1, 1, 0, net::ERR_NAME_NOT_RESOLVED, false},
      {2, 2, true, 1, 2, 0, net::ERR_NAME_NOT_RESOLVED, false},
      {2, 0, true, 1, 1, 1, net::ERR_NAME_NOT_RESOLVED, false},
      {2, 0, false, 1, 1, 0, net::ERR_NAME_NOT_RESOLVED, false},
  };

  SetStaleDelay(kNoStaleDelaySec);
  CreateNetworkChangeNotifier();

  for (size_t i = 0; i < arraysize(kUsabilityTestCases); ++i) {
    const auto& test_case = kUsabilityTestCases[i];

    SetStaleUsability(test_case.max_expired_time_sec, test_case.max_stale_uses,
                      test_case.allow_other_network);
    CreateResolver();
    CreateCacheEntry(kCacheEntryTTLSec + test_case.age_sec, test_case.error);
    for (int j = 0; j < test_case.network_changes; ++j)
      OnNetworkChange();
    for (int j = 0; j < test_case.stale_use - 1; ++j)
      LookupStale();

    Resolve();
    WaitForResolve();
    EXPECT_TRUE(resolve_complete()) << i;

    if (test_case.error == net::OK) {
      EXPECT_EQ(test_case.error, resolve_error()) << i;
      EXPECT_EQ(1u, resolve_addresses().size()) << i;
      {
        const char* expected =
            test_case.usable ? kCacheAddress : kNetworkAddress;
        EXPECT_EQ(expected, resolve_addresses()[0].ToStringWithoutPort()) << i;
      }
    } else {
      if (test_case.usable) {
        EXPECT_EQ(test_case.error, resolve_error()) << i;
      } else {
        EXPECT_EQ(net::OK, resolve_error()) << i;
        EXPECT_EQ(1u, resolve_addresses().size()) << i;
        EXPECT_EQ(kNetworkAddress, resolve_addresses()[0].ToStringWithoutPort())
            << i;
      }
    }

    DestroyResolver();
  }
}

TEST_F(StaleHostResolverTest, CreatedByContext) {
  URLRequestContextConfig config(
      // Enable QUIC.
      true,
      // QUIC User Agent ID.
      "Default QUIC User Agent ID",
      // Enable SPDY.
      true,
      // Enable Brotli.
      false,
      // Type of http cache.
      URLRequestContextConfig::HttpCacheType::DISK,
      // Max size of http cache in bytes.
      1024000,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      false,
      // Storage path for http cache and cookie storage.
      "/data/data/org.chromium.net/app_cronet_test/test_storage",
      // User-Agent request header field.
      "fake agent",
      // JSON encoded experimental options.
      "{\"AsyncDNS\":{\"enable\":false},"
      "\"StaleDNS\":{\"enable\":true,"
      "\"delay_ms\":0,"
      "\"max_expired_time_ms\":0,"
      "\"max_stale_uses\":0}}",
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier>(),
      // Enable network quality estimator.
      false,
      // Enable Public Key Pinning bypass for local trust anchors.
      true,
      // Certificate verifier cache data.
      "");

  net::URLRequestContextBuilder builder;
  net::NetLog net_log;
  config.ConfigureURLRequestContextBuilder(&builder, &net_log);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(base::WrapUnique(
      new net::ProxyConfigServiceFixed(net::ProxyConfig::CreateDirect())));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());

  // Duplicate StaleCache test case to ensure StaleHostResolver was created:

  // Note: Experimental config above sets 0ms stale delay.
  SetResolver(context->host_resolver());
  CreateCacheEntry(kAgeExpiredSec, net::OK);

  Resolve();
  EXPECT_FALSE(resolve_complete());
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(net::OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());
}

}  // namespace

}  // namespace cronet
