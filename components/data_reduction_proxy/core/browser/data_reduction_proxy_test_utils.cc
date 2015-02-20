// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/url_request/url_request_test_util.h"

namespace data_reduction_proxy {

TestDataReductionProxyIOData::TestDataReductionProxyIOData(
    DataReductionProxySettings* settings,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_ptr<DataReductionProxyEventStore> event_store,
    scoped_ptr<DataReductionProxyRequestOptions> request_options,
    scoped_ptr<DataReductionProxyConfigurator> configurator)
    : DataReductionProxyIOData() {
  settings_ = settings;
  params_ = settings_->params()->Clone();
  event_store_ = event_store.Pass();
  request_options_ = request_options.Pass();
  configurator_ = configurator.Pass();
  io_task_runner_ = task_runner;
  ui_task_runner_ = task_runner;
}

TestDataReductionProxyIOData::~TestDataReductionProxyIOData() {
  shutdown_on_ui_ = true;
}

DataReductionProxyTestContext::DataReductionProxyTestContext(
    int params_flags,
    unsigned int params_definitions,
    unsigned int test_context_flags)
    : test_context_flags_(test_context_flags),
      task_runner_(base::MessageLoopProxy::current()),
      request_context_(scoped_refptr<net::URLRequestContextGetter>(
          new net::TestURLRequestContextGetter(task_runner_))),
      statistics_prefs_pointer_(nullptr) {
  scoped_ptr<DataReductionProxyEventStore> event_store =
      make_scoped_ptr(new DataReductionProxyEventStore(task_runner_));
  scoped_ptr<DataReductionProxyConfigurator> configurator;
  if (test_context_flags_ &
      DataReductionProxyTestContext::USE_TEST_CONFIGURATOR) {
    configurator = make_scoped_ptr(new TestDataReductionProxyConfigurator(
        task_runner_, &net_log_, event_store.get()));
  } else {
    configurator = make_scoped_ptr(new DataReductionProxyConfigurator(
        task_runner_, &net_log_, event_store.get()));
  }

  scoped_ptr<TestDataReductionProxyConfig> config;
  if (test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_CONFIG) {
    config.reset(new MockDataReductionProxyConfig(params_flags));
  } else {
    config.reset(new TestDataReductionProxyConfig(params_flags));
  }

  settings_.reset(new DataReductionProxySettings(make_scoped_ptr(
      new DataReductionProxyParams(params_flags))));
  settings_->config_ = config.Pass();

  scoped_ptr<DataReductionProxyRequestOptions> request_options =
      make_scoped_ptr(new DataReductionProxyRequestOptions(
          Client::UNKNOWN, settings_->config_->params(), task_runner_));
  io_data_.reset(new TestDataReductionProxyIOData(
      settings_.get(), task_runner_.get(), event_store.Pass(),
      request_options.Pass(), configurator.Pass()));
  settings_->config_->InitDataReductionProxyConfig(
      task_runner_, &net_log_, request_context_.get(),
      io_data_->configurator(), io_data_->event_store());
  if (!(test_context_flags_ &
        DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION)) {
    scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs =
        make_scoped_ptr(new DataReductionProxyStatisticsPrefs(
            &simple_pref_service_, task_runner_, base::TimeDelta()));
    statistics_prefs_pointer_ = statistics_prefs.get();
    io_data_->SetDataReductionProxyStatisticsPrefs(
        statistics_prefs_pointer_->GetWeakPtr());
  }
}

DataReductionProxyTestContext::~DataReductionProxyTestContext() {
}

void DataReductionProxyTestContext::RunUntilIdle() {
  base::MessageLoop::current()->RunUntilIdle();
}

void DataReductionProxyTestContext::InitSettings() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  settings_->InitDataReductionProxySettings(
      &simple_pref_service_, CreateStatisticsPrefs(), request_context_.get(),
      io_data_->net_log(), io_data_->event_store(), false);
  io_data_->SetDataReductionProxyStatisticsPrefs(settings_->statistics_prefs());
}

scoped_ptr<DataReductionProxyStatisticsPrefs>
DataReductionProxyTestContext::CreateStatisticsPrefs() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  return make_scoped_ptr(new DataReductionProxyStatisticsPrefs(
      &simple_pref_service_, task_runner_, base::TimeDelta()));
}

TestDataReductionProxyConfigurator*
DataReductionProxyTestContext::test_configurator() const {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_TEST_CONFIGURATOR);
  return reinterpret_cast<TestDataReductionProxyConfigurator*>(
      io_data_->configurator());
}

MockDataReductionProxyConfig* DataReductionProxyTestContext::mock_config()
    const {
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_CONFIG);
  return reinterpret_cast<MockDataReductionProxyConfig*>(
      settings_->config_.get());
}

DataReductionProxyStatisticsPrefs*
DataReductionProxyTestContext::statistics_prefs()
    const {
  DCHECK(!(test_context_flags_ &
           DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION));
  return statistics_prefs_pointer_;
}

}  // namespace data_reduction_proxy
