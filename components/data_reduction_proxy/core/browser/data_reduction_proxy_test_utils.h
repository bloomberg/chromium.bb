// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "net/base/capturing_net_log.h"
#include "testing/gmock/include/gmock/gmock.h"

class TestingPrefServiceSimple;

namespace base {
class MessageLoopForUI;
class SingleThreadTaskRunner;
}

namespace net {
class NetLog;
class URLRequestContext;
class URLRequestContextGetter;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyRequestOptions;
class DataReductionProxySettings;
class DataReductionProxyStatisticsPrefs;
class MockDataReductionProxyConfig;
class TestDataReductionProxyConfig;
class TestDataReductionProxyConfigurator;

// Test version of |DataReductionProxyService|, which permits mocking of various
// methods.
class MockDataReductionProxyService : public DataReductionProxyService {
 public:
  MockDataReductionProxyService(
      scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs,
      DataReductionProxySettings* settings,
      net::URLRequestContextGetter* request_context);
  ~MockDataReductionProxyService() override;

  MOCK_METHOD2(SecureProxyCheck,
      void(const GURL& secure_proxy_check_url,
           FetcherResponseCallback fetcher_callback));
};

// Test version of |DataReductionProxyIOData|, which bypasses initialization in
// the constructor in favor of explicitly passing in its owned classes. This
// permits the use of test/mock versions of those classes.
class TestDataReductionProxyIOData : public DataReductionProxyIOData {
 public:
  TestDataReductionProxyIOData(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_ptr<TestDataReductionProxyConfig> config,
      scoped_ptr<DataReductionProxyEventStore> event_store,
      scoped_ptr<DataReductionProxyRequestOptions> request_options,
      scoped_ptr<DataReductionProxyConfigurator> configurator);
  ~TestDataReductionProxyIOData() override;

  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }
};

// Builds a test version of the Data Reduction Proxy stack for use in tests.
// Takes in various |TestContextOptions| which controls the behavior of the
// underlying objects.
class DataReductionProxyTestContext {
 public:
  // Allows for a fluent builder interface to configure what kind of objects
  // (test vs mock vs real) are used by the |DataReductionProxyTestContext|.
  class Builder {
   public:
    Builder();

    // |DataReductionProxyParams| flags to use.
    Builder& WithParamsFlags(int params_flags);

    // |TestDataReductionProxyParams| flags to use.
    Builder& WithParamsDefinitions(unsigned int params_definitions);

    // The |Client| enum to use for |DataReductionProxyRequestOptions|.
    Builder& WithClient(Client client);

    // Specifies a |net::URLRequestContext| to use. The |request_context| is
    // owned by the caller.
    Builder& WithURLRequestContext(net::URLRequestContext* request_context);

    // Specifies the use of |MockDataReductionProxyConfig| instead of
    // |TestDataReductionProxyConfig|.
    Builder& WithMockConfig();

    // Specifies the use of |TestDataReductionProxyConfigurator| instead of
    // |DataReductionProxyConfigurator|.
    Builder& WithTestConfigurator();

    // Specifies the use of |MockDataReductionProxyService| instead of
    // |DataReductionProxyService|.
    Builder& WithMockDataReductionProxyService();

    // Construct, but do not initialize the |DataReductionProxySettings| object.
    Builder& SkipSettingsInitialization();

    // Creates a |DataReductionProxyTestContext|. Owned by the caller.
    scoped_ptr<DataReductionProxyTestContext> Build();

   private:
    int params_flags_;
    unsigned int params_definitions_;
    Client client_;
    net::URLRequestContext* request_context_;

    bool use_mock_config_;
    bool use_test_configurator_;
    bool use_mock_service_;
    bool skip_settings_initialization_;
  };

  virtual ~DataReductionProxyTestContext();

  // Waits while executing all tasks on the current SingleThreadTaskRunner.
  void RunUntilIdle();

  // Initializes the |DataReductionProxySettings| object. Can only be called if
  // built with SkipSettingsInitialization.
  void InitSettings();

  // Creates a |DataReductionProxyService| object, or a
  // |MockDataReductionProxyService| if built with
  // WithMockDataReductionProxyService. Can only be called if built with
  // SkipSettingsInitialization.
  scoped_ptr<DataReductionProxyService> CreateDataReductionProxyService();

  // Returns the underlying |TestDataReductionProxyConfigurator|. This can only
  // be called if built with WithTestConfigurator.
  TestDataReductionProxyConfigurator* test_configurator() const;

  // Returns the underlying |MockDataReductionProxyConfig|. This can only be
  // called if built with WithMockConfig.
  MockDataReductionProxyConfig* mock_config() const;

  DataReductionProxyService* data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyService|. This can only
  // be called if built with WithMockDataReductionProxyService.
  MockDataReductionProxyService* mock_data_reduction_proxy_service() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  TestingPrefServiceSimple* pref_service() {
    return simple_pref_service_.get();
  }

  net::NetLog* net_log() {
    return net_log_.get();
  }

  net::URLRequestContextGetter* request_context_getter() const {
    return request_context_getter_.get();
  }

  DataReductionProxyEventStore* event_store() const {
    return io_data_->event_store();
  }

  DataReductionProxyConfigurator* configurator() const {
    return io_data_->configurator();
  }

  TestDataReductionProxyConfig* config() const {
    return reinterpret_cast<TestDataReductionProxyConfig*>(io_data_->config());
  }

  TestDataReductionProxyIOData* io_data() const {
    return io_data_.get();
  }

  DataReductionProxySettings* settings() const {
    return settings_.get();
  }

 private:
  enum TestContextOptions {
    // Permits mocking of the underlying |DataReductionProxyConfig|.
    USE_MOCK_CONFIG = 0x1,
    // Uses a |TestDataReductionProxyConfigurator| to record proxy configuration
    // changes.
    USE_TEST_CONFIGURATOR = 0x2,
    // Construct, but do not initialize the |DataReductionProxySettings| object.
    // Primarily used for testing of the |DataReductionProxySettings| object
    // itself.
    SKIP_SETTINGS_INITIALIZATION = 0x4,
    // Permits mocking of the underlying |DataReductionProxyService|.
    USE_MOCK_SERVICE = 0x8,
  };

  DataReductionProxyTestContext(
      scoped_ptr<base::MessageLoop> loop,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_ptr<TestingPrefServiceSimple> simple_pref_service,
      scoped_ptr<net::CapturingNetLog> net_log,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_ptr<TestDataReductionProxyIOData> io_data,
      scoped_ptr<DataReductionProxySettings> settings,
      unsigned int test_context_flags);

  void InitSettingsWithoutCheck();

  scoped_ptr<DataReductionProxyService>
      CreateDataReductionProxyServiceInternal();

  unsigned int test_context_flags_;

  scoped_ptr<base::MessageLoop> loop_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<TestingPrefServiceSimple> simple_pref_service_;
  scoped_ptr<net::CapturingNetLog> net_log_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  scoped_ptr<TestDataReductionProxyIOData> io_data_;
  scoped_ptr<DataReductionProxySettings> settings_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyTestContext);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
