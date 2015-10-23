// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/content/data_use_measurement.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/test/histogram_tester.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace data_use_measurement {

class DataUseMeasurementTest : public testing::Test {
 public:
  DataUseMeasurementTest() {
    // During the test it is expected to not have cellular connection.
    DCHECK(!net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType()));
  }

  // This function makes a user request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAUserRequest(const std::string& target_dimension) {
    net::TestDelegate test_delegate;
    InitializeContext();
    base::HistogramTester histogram_tester;
    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 OK\r\n"
                                           "Content-Length: 12\r\n\r\n"),
                             net::MockRead("Test Content")};
    net::StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr,
                                              0);
    socket_factory_->AddSocketDataProvider(&socket_data);

    scoped_ptr<net::URLRequest> request(context_->CreateRequest(
        GURL("http://foo.com"), net::DEFAULT_PRIORITY, &test_delegate));
    request->SetUserData(
        data_use_measurement::DataUseUserData::kUserDataKey,
        new data_use_measurement::DataUseUserData(
            data_use_measurement::DataUseUserData::SUGGESTIONS));
    request->Start();
    loop_.RunUntilIdle();

    data_use_measurement_.ReportDataUseUMA(request.get());
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
    // One upload and one download message, so total count should be 2.
    histogram_tester.ExpectTotalCount("DataUse.MessageSize.Suggestions", 2);
  }

  // This function makes a service request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAServiceRequest(const std::string& target_dimension) {
    net::TestDelegate test_delegate;
    InitializeContext();
    base::HistogramTester histogram_tester;

    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 OK\r\n"
                                           "Content-Length: 12\r\n\r\n"),
                             net::MockRead("Test Content")};
    net::StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr,
                                              0);
    socket_factory_->AddSocketDataProvider(&socket_data);

    scoped_ptr<net::URLRequest> request(context_->CreateRequest(
        GURL("http://foo.com"), net::DEFAULT_PRIORITY, &test_delegate));
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_MAIN_FRAME, nullptr, -2, -2, -2,
        true, false, true, true, false);
    request->Start();
    loop_.RunUntilIdle();

    data_use_measurement_.ReportDataUseUMA(request.get());
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount(
        "DataUse.MessageSize.AllServices.Upstream." + target_dimension +
            kConnectionType,
        0);
    histogram_tester.ExpectTotalCount(
        "DataUse.MessageSize.AllServices.Downstream." + target_dimension +
            kConnectionType,
        0);
  }

  DataUseMeasurement* data_use_measurement() { return &data_use_measurement_; }

 private:
  void InitializeContext() {
    context_.reset(new net::TestURLRequestContext(true));
    socket_factory_.reset(new net::MockClientSocketFactory());
    context_->set_client_socket_factory(socket_factory_.get());
    context_->Init();
  }

  base::MessageLoopForIO loop_;
  DataUseMeasurement data_use_measurement_;
  scoped_ptr<net::MockClientSocketFactory> socket_factory_;
  scoped_ptr<net::TestURLRequestContext> context_;
  const std::string kConnectionType = "NotCellular";

  DISALLOW_COPY_AND_ASSIGN(DataUseMeasurementTest);
};

// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// foreground or the OS is not Android.
// TODO(amohammadkhan): Add tests for Cellular/non-cellular connection types
// when support for testing is provided in its class.
TEST_F(DataUseMeasurementTest, UserNotUserTest) {
#if defined(OS_ANDROID)
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
#endif
  TestForAServiceRequest("Foreground.");
  TestForAUserRequest("Foreground.");
}

#if defined(OS_ANDROID)
// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// background and OS is Android.
TEST_F(DataUseMeasurementTest, ApplicationStateTest) {
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  TestForAServiceRequest("Background.");
  TestForAUserRequest("Background.");
}
#endif

}  // namespace data_use_measurement
