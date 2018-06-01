// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"

#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// URL prefix for spec requests.
#define SERVER_URL "https://www.gstatic.com/chrome/password_requirements/"

TEST(PasswordRequirementsSpecFetcherTest, FetchData) {
  // An empty spec is returned for all error cases (time outs, server responding
  // with anything but HTTP_OK).
  PasswordRequirementsSpec empty_spec;

  // TODO(crbug.com/846694): As there is currently no wire-format for files
  // that contain multiple specs, the HTTP response from the server is ignored
  // and any HTTP_OK response generates a PasswordRequirementsSpec with a magic
  // value of "min_length = 17". Once the wire-format is defined, this should
  // be updated.
  constexpr char kSuccessfulResponseContent[] = "Foobar";
  PasswordRequirementsSpec success_spec;
  success_spec.set_min_length(17);

  // If this magic timeout value is set, simulate a timeout.
  const int kMagicTimeout = 10;

  struct {
    // Name of the test for log output.
    const char* test_name;

    // Origin for which the spec is fetched.
    const char* origin;

    // Current configuration that would be set via Finch.
    int generation = 1;
    int prefix_length = 32;
    int timeout = 1000;

    // Handler for the spec requests.
    const char* requested_url;
    const char* response_content;
    net::HttpStatusCode response_status = net::HTTP_OK;

    // Expected spec.
    PasswordRequirementsSpec* expected_spec;
  } tests[] = {
      {
          .test_name = "Business as usual",
          .origin = "https://www.example.com",
          // See echo -n example.com | md5sum | cut -b 1-4
          .requested_url = SERVER_URL "1/5aba",
          .response_content = kSuccessfulResponseContent,
          .expected_spec = &success_spec,
      },
      {
          .test_name = "Parts before the eTLD+1 don't matter",
          // m.example.com instead of www.example.com creates the same hash
          // prefix.
          .origin = "https://m.example.com",
          .requested_url = SERVER_URL "1/5aba",
          .response_content = kSuccessfulResponseContent,
          .expected_spec = &success_spec,
      },
      {
          .test_name = "The generation is encoded in the url",
          .origin = "https://www.example.com",
          // Here the test differs from the default:
          .generation = 2,
          .requested_url = SERVER_URL "2/5aba",
          .response_content = kSuccessfulResponseContent,
          .expected_spec = &success_spec,
      },
      {
          .test_name = "Shorter prefixes are reflected in the URL",
          .origin = "https://m.example.com",
          // The prefix "5abc" starts with 0b01011010. If the prefix is limited
          // to the first 3 bits, b0100 = 0x4 remains and the rest is zeroed
          // out.
          .prefix_length = 3,
          .requested_url = SERVER_URL "1/4000",
          .response_content = kSuccessfulResponseContent,
          .expected_spec = &success_spec,
      },
      {
          .test_name = "Simulate a 404 response",
          .origin = "https://www.example.com",
          .requested_url = SERVER_URL "1/5aba",
          .response_content = "Not found",
          // If a file is not found on the server, the spec should be empty.
          .response_status = net::HTTP_NOT_FOUND,
          .expected_spec = &empty_spec,
      },
      {
          .test_name = "Simulate a timeout",
          .origin = "https://www.example.com",
          // This simulates a timeout.
          .timeout = kMagicTimeout,
          // This makes sure that the server does not respond by itself as
          // TestURLLoaderFactory reacts as if a response has been added to
          // a URL.
          .requested_url = SERVER_URL "dont_respond",
          .response_content = kSuccessfulResponseContent,
          .expected_spec = &empty_spec,
      },
      {
          .test_name = "Zero prefix",
          .origin = "https://www.example.com",
          // A zero prefix will be the hard-coded Finch default and should work.
          .prefix_length = 0,
          .requested_url = SERVER_URL "1/0000",
          .response_content = kSuccessfulResponseContent,
          .expected_spec = &success_spec,
      },
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.test_name);

    base::test::ScopedTaskEnvironment environment(
        base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
    network::TestURLLoaderFactory loader_factory;
    loader_factory.AddResponse(test.requested_url, test.response_content,
                               test.response_status);

    // Data to be collected from the callback.
    bool callback_called = false;
    PasswordRequirementsSpec returned_spec;

    // Trigger the network request and record data of the callback.
    PasswordRequirementsSpecFetcher fetcher(test.generation, test.prefix_length,
                                            test.timeout);
    auto callback =
        base::BindLambdaForTesting([&](const PasswordRequirementsSpec& spec) {
          callback_called = true;
          returned_spec = spec;
        });
    fetcher.Fetch(&loader_factory, GURL(test.origin), callback);

    environment.RunUntilIdle();

    if (test.timeout == kMagicTimeout) {
      // Make sure that the request takes longer than the timeout and gets
      // killed by the timer.
      environment.FastForwardBy(
          base::TimeDelta::FromMilliseconds(2 * kMagicTimeout));
      environment.RunUntilIdle();
    }

    ASSERT_TRUE(callback_called);
    EXPECT_EQ(test.expected_spec->SerializeAsString(),
              returned_spec.SerializeAsString());
  }
}

#undef SERVER_URL

}  // namespace

}  // namespace autofill
