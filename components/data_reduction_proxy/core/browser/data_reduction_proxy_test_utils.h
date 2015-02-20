// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_

#include "base/macros.h"
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

  MOCK_METHOD2(CheckProbeURL,
      void(const GURL& probe_url, FetcherResponseCallback fetcher_callback));
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
  static const unsigned int DEFAULT_TEST_CONTEXT_OPTIONS = 0;

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
  };

  // Creates a new DataReductionProxyTestContext. |params_flags| controls what
  // is enabled in the underlying |DataReductionProxyParams|.
  // |params_definitions| is used to control the HasNames enum for the
  // underlying |TestDataReductionProxyParams|. |test_context_flags| is the
  // |TestContextOptions| enum to control what underlying objects are created.
  explicit DataReductionProxyTestContext(int params_flags,
                                         unsigned int params_definitions,
                                         unsigned int test_context_flags);

  virtual ~DataReductionProxyTestContext();

  // Waits while executing all tasks on the current SingleThreadTaskRunner.
  void RunUntilIdle();

  // Initializes the |DataReductionProxySettings| object. Can only be called if
  // |SKIP_SETTINGS_INITIALIZATION| was specified.
  void InitSettings();

  // Creates a |MockDataReductionProxyService| object. Can only be called if
  // |SKIP_SETTINGS_INITIALIZATION| was specified.
  scoped_ptr<MockDataReductionProxyService> CreateDataReductionProxyService();

  // Returns the underlying |TestDataReductionProxyConfigurator|. This can only
  // be called if |USE_TEST_CONFIGURATOR| was specified.
  TestDataReductionProxyConfigurator* test_configurator() const;

  // Returns the underlying |MockDataReductionProxyConfig|. This can only be
  // called if |USE_MOCK_CONFIG| was specified.
  MockDataReductionProxyConfig* mock_config() const;

  // Returns the underlying |MockDataReductionProxyService|. This can only
  // be called if |SKIP_SETTINGS_INITIALIZATION| was not specified.
  MockDataReductionProxyService* data_reduction_proxy_service() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  TestingPrefServiceSimple* pref_service() {
    return &simple_pref_service_;
  }

  net::NetLog* net_log() {
    return &net_log_;
  }

  net::URLRequestContextGetter* request_context() const {
    return request_context_.get();
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
  unsigned int test_context_flags_;

  base::MessageLoopForIO loop_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TestingPrefServiceSimple simple_pref_service_;
  net::CapturingNetLog net_log_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  scoped_ptr<TestDataReductionProxyIOData> io_data_;
  scoped_ptr<DataReductionProxySettings> settings_;

  MockDataReductionProxyService* service_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyTestContext);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
