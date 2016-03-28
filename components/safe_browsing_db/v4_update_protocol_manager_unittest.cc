// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "components/safe_browsing_db/v4_update_protocol_manager.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";

}  // namespace

namespace safe_browsing {

typedef V4UpdateProtocolManager::ListUpdateRequest ListUpdateRequest;
typedef V4UpdateProtocolManager::ListUpdateResponse ListUpdateResponse;

class V4UpdateProtocolManagerTest : public testing::Test {
 protected:
  scoped_ptr<V4UpdateProtocolManager> CreateProtocolManager() {
    V4ProtocolConfig config;
    config.client_name = kClient;
    config.version = kAppVer;
    config.key_param = kKeyParam;
    return scoped_ptr<V4UpdateProtocolManager>(
        V4UpdateProtocolManager::Create(NULL, config));
  }

  void SetupListsToUpdate(
      base::hash_set<UpdateListIdentifier>* lists_to_update) {
    UpdateListIdentifier list_identifier;
    list_identifier.platform_type = WINDOWS_PLATFORM;
    list_identifier.threat_entry_type = URL_EXPRESSION;
    list_identifier.threat_type = MALWARE_THREAT;
    lists_to_update->insert(list_identifier);

    list_identifier.platform_type = WINDOWS_PLATFORM;
    list_identifier.threat_entry_type = URL_EXPRESSION;
    list_identifier.threat_type = UNWANTED_SOFTWARE;
    lists_to_update->insert(list_identifier);

    list_identifier.platform_type = WINDOWS_PLATFORM;
    list_identifier.threat_entry_type = BINARY_DIGEST;
    list_identifier.threat_type = MALWARE_THREAT;
    lists_to_update->insert(list_identifier);
  }

  void ClearListsToUpdate(
      base::hash_set<UpdateListIdentifier>* lists_to_update) {
    lists_to_update->clear();
  }

  void SetupCurrentListStates(
      const base::hash_set<UpdateListIdentifier>& lists_to_update,
      base::hash_map<UpdateListIdentifier, std::string>* current_list_states) {
    // TODO(vakh): Implement this to test the cases when we have an existing
    // state for some of the lists.
  }

  std::string GetStockV4UpdateResponse() {
    FetchThreatListUpdatesResponse response;

    ListUpdateResponse* lur = response.add_list_update_responses();
    lur->set_platform_type(WINDOWS_PLATFORM);
    lur->set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
    lur->set_threat_entry_type(URL_EXPRESSION);
    lur->set_threat_type(MALWARE_THREAT);

    lur = response.add_list_update_responses();
    lur->set_platform_type(WINDOWS_PLATFORM);
    lur->set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
    lur->set_threat_entry_type(URL_EXPRESSION);
    lur->set_threat_type(UNWANTED_SOFTWARE);

    lur = response.add_list_update_responses();
    lur->set_platform_type(WINDOWS_PLATFORM);
    lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
    lur->set_threat_entry_type(BINARY_DIGEST);
    lur->set_threat_type(MALWARE_THREAT);

    // Serialize.
    std::string res_data;
    response.SerializeToString(&res_data);

    return res_data;
  }
};

void ValidateGetUpdatesResults(
    const std::vector<ListUpdateResponse>& expected_lurs,
    const std::vector<ListUpdateResponse>& list_update_responses) {
  ASSERT_EQ(expected_lurs.size(), list_update_responses.size());

  for (unsigned int i = 0; i < list_update_responses.size(); ++i) {
    const ListUpdateResponse& expected = expected_lurs[i];
    const ListUpdateResponse& actual = list_update_responses[i];

    EXPECT_EQ(expected.platform_type(), actual.platform_type());
    EXPECT_EQ(expected.response_type(), actual.response_type());
    EXPECT_EQ(expected.threat_entry_type(), actual.threat_entry_type());
    EXPECT_EQ(expected.threat_type(), actual.threat_type());

    // TODO(vakh): Test more fields from the proto.
  }
}

// TODO(vakh): Add many more tests.
#if defined(OS_ANDROID)
// Flakes on Android: https://crbug.com/598412
#define MAYBE_TestGetUpdatesErrorHandlingNetwork \
        DISABLED_TestGetUpdatesErrorHandlingNetwork
#else
#define MAYBE_TestGetUpdatesErrorHandlingNetwork \
        TestGetUpdatesErrorHandlingNetwork
#endif
TEST_F(V4UpdateProtocolManagerTest, MAYBE_TestGetUpdatesErrorHandlingNetwork) {
  net::TestURLFetcherFactory factory;
  scoped_ptr<V4UpdateProtocolManager> pm(CreateProtocolManager());

  const std::vector<ListUpdateResponse> expected_lurs;
  const base::hash_set<UpdateListIdentifier> lists_to_update;
  const base::hash_map<UpdateListIdentifier, std::string> current_list_states;
  pm->GetUpdates(lists_to_update, current_list_states,
                 base::Bind(&ValidateGetUpdatesResults, expected_lurs));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  // Failed request status should result in error.
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_CONNECTION_RESET));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
}

#if defined(OS_ANDROID)
// Flakes on Android: https://crbug.com/598412
#define MAYBE_TestGetUpdatesErrorHandlingResponseCode \
        DISABLED_TestGetUpdatesErrorHandlingResponseCode
#else
#define MAYBE_TestGetUpdatesErrorHandlingResponseCode \
        TestGetUpdatesErrorHandlingResponseCode
#endif
TEST_F(V4UpdateProtocolManagerTest, \
       MAYBE_TestGetUpdatesErrorHandlingResponseCode) {
  net::TestURLFetcherFactory factory;
  scoped_ptr<V4UpdateProtocolManager> pm(CreateProtocolManager());

  const std::vector<ListUpdateResponse> expected_lurs;
  const base::hash_set<UpdateListIdentifier> lists_to_update;
  const base::hash_map<UpdateListIdentifier, std::string> current_list_states;
  pm->GetUpdates(lists_to_update, current_list_states,
                 base::Bind(&ValidateGetUpdatesResults, expected_lurs));


  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  // Response code of anything other than 200 should result in error.
  fetcher->set_response_code(204);
  fetcher->SetResponseString(GetStockV4UpdateResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
}


#if defined(OS_ANDROID)
// Flakes on Android: https://crbug.com/598412
#define MAYBE_TestGetUpdatesNoError \
        DISABLED_TestGetUpdatesNoError
#else
#define MAYBE_TestGetUpdatesNoError \
        TestGetUpdatesNoError
#endif
TEST_F(V4UpdateProtocolManagerTest, MAYBE_TestGetUpdatesNoError) {
  net::TestURLFetcherFactory factory;
  scoped_ptr<V4UpdateProtocolManager> pm(CreateProtocolManager());


  const std::vector<ListUpdateResponse> expected_lurs;
  base::hash_set<UpdateListIdentifier> lists_to_update;
  SetupListsToUpdate(&lists_to_update);
  base::hash_map<UpdateListIdentifier, std::string> current_list_states;
  SetupCurrentListStates(lists_to_update, &current_list_states);
  pm->GetUpdates(lists_to_update, current_list_states,
                 base::Bind(&ValidateGetUpdatesResults, expected_lurs));
  ClearListsToUpdate(&lists_to_update);

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4UpdateResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
}

}  // namespace safe_browsing
