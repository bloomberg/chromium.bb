// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class TestNetworkPropertiesManager : public NetworkPropertiesManager {
 public:
  TestNetworkPropertiesManager(
      PrefService* pref_service,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : NetworkPropertiesManager(pref_service, ui_task_runner) {}
  ~TestNetworkPropertiesManager() override {}
};

TEST(NetworkPropertyTest, TestSetterGetterCaptivePortal) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  std::string network_id("test");
  network_properties_manager.OnChangeInNetworkID(network_id);

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(
      network_properties_manager.HasWarmupURLProbeFailed(false, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
  base::RunLoop().RunUntilIdle();

  // Verify the prefs.
  EXPECT_EQ(1u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kNetworkProperties)->HasKey(network_id));
  {
    TestNetworkPropertiesManager network_properties_manager_2(
        &test_prefs, base::ThreadTaskRunnerHandle::Get());
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_TRUE(network_properties_manager_2.IsCaptivePortal());
  }

  network_properties_manager.SetIsCaptivePortal(false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(
      network_properties_manager.HasWarmupURLProbeFailed(false, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
  base::RunLoop().RunUntilIdle();

  {
    TestNetworkPropertiesManager network_properties_manager_2(
        &test_prefs, base::ThreadTaskRunnerHandle::Get());
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_FALSE(network_properties_manager_2.IsCaptivePortal());
  }

  // Verify the prefs.
  EXPECT_EQ(1u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kNetworkProperties)->HasKey(network_id));
}

TEST(NetworkPropertyTest, TestSetterGetterDisallowedByCarrier) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));

  network_properties_manager.SetIsSecureProxyDisallowedByCarrier(true);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(
      network_properties_manager.HasWarmupURLProbeFailed(false, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));

  network_properties_manager.SetIsSecureProxyDisallowedByCarrier(false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(
      network_properties_manager.HasWarmupURLProbeFailed(false, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
}

TEST(NetworkPropertyTest, TestWarmupURLFailedOnSecureCoreProxy) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));

  network_properties_manager.SetHasWarmupURLProbeFailed(
      true /* secure_proxy */, true /* is_core_proxy */,
      true /* warmup_url_probe_failed */);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(
      false /* secure_proxy */, false /* is_core_proxy */));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_TRUE(network_properties_manager.HasWarmupURLProbeFailed(true, true));

  network_properties_manager.SetHasWarmupURLProbeFailed(true, true, false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(
      network_properties_manager.HasWarmupURLProbeFailed(false, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));
}

TEST(NetworkPropertyTest, TestWarmupURLFailedOnInSecureCoreProxy) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

  network_properties_manager.SetHasWarmupURLProbeFailed(
      false /* secure_proxy */, true /* is_core_proxy */,
      true /* warmup_url_probe_failed */);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(
      false /* secure_proxy */, false /* is_core_proxy */));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
  EXPECT_TRUE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));

  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(false));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(
      network_properties_manager.HasWarmupURLProbeFailed(false, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, false));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));
}

TEST(NetworkPropertyTest, TestLimitPrefSize) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  size_t num_network_ids = 100;

  for (size_t i = 0; i < num_network_ids; ++i) {
    std::string network_id("test" + base::IntToString(i));
    network_properties_manager.OnChangeInNetworkID(network_id);

    // State should be reset when there is a change in the network ID.
    EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
    EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

    network_properties_manager.SetIsCaptivePortal(true);
    EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
    EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
    EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
    EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
    EXPECT_FALSE(
        network_properties_manager.HasWarmupURLProbeFailed(false, true));
    EXPECT_FALSE(
        network_properties_manager.HasWarmupURLProbeFailed(true, true));

    base::RunLoop().RunUntilIdle();
  }

  // Pref size should be bounded to 10.
  EXPECT_EQ(10u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));

  // The last 10 network IDs are guaranteed to be present in the prefs.
  for (size_t i = num_network_ids - 10; i < num_network_ids; ++i) {
    EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                    ->HasKey("test" + base::IntToString(i)));
  }

  {
    TestNetworkPropertiesManager network_properties_manager_2(
        &test_prefs, base::ThreadTaskRunnerHandle::Get());
    for (size_t i = 0; i < num_network_ids; ++i) {
      std::string network_id("test" + base::IntToString(i));
      network_properties_manager_2.OnChangeInNetworkID(network_id);

      EXPECT_EQ(test_prefs.GetDictionary(prefs::kNetworkProperties)
                    ->HasKey(network_id),
                network_properties_manager_2.IsCaptivePortal());
      EXPECT_FALSE(
          network_properties_manager_2.IsSecureProxyDisallowedByCarrier());
      EXPECT_FALSE(
          network_properties_manager_2.HasWarmupURLProbeFailed(false, true));
      EXPECT_FALSE(
          network_properties_manager_2.HasWarmupURLProbeFailed(true, true));
    }
  }
}

TEST(NetworkPropertyTest, TestChangeNetworkIDBackAndForth) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  // First network ID has a captive portal.
  std::string first_network_id("test1");
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Warmup probe failed on the insecure proxy for the second network ID.
  std::string second_network_id("test2");
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, true);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Change back to |first_network_id|.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));

  // Change back to |second_network_id|.
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));

  // Verify the prefs.
  EXPECT_EQ(2u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(first_network_id));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(second_network_id));
}

TEST(NetworkPropertyTest, TestNetworkQualitiesOverwrite) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  // First network ID has a captive portal.
  std::string first_network_id("test1");
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Warmup probe failed on the insecure proxy for the second network ID.
  std::string second_network_id("test2");
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, true);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Change back to |first_network_id|.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, true);

  // Change to |first_network_id|.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));

  // Verify the prefs.
  EXPECT_EQ(2u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(first_network_id));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(second_network_id));
}

TEST(NetworkPropertyTest, TestDeleteHistory) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::MessageLoopForIO loop;
  TestNetworkPropertiesManager network_properties_manager(
      &test_prefs, base::ThreadTaskRunnerHandle::Get());

  std::string network_id("test");
  network_properties_manager.OnChangeInNetworkID(network_id);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.NetworkProperties.CacheHit", false, 1);

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));
  base::RunLoop().RunUntilIdle();

  // Verify the prefs.
  EXPECT_EQ(1u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kNetworkProperties)->HasKey(network_id));
  {
    TestNetworkPropertiesManager network_properties_manager_2(
        &test_prefs, base::ThreadTaskRunnerHandle::Get());
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_TRUE(network_properties_manager_2.IsCaptivePortal());
    histogram_tester.ExpectBucketCount(
        "DataReductionProxy.NetworkProperties.CacheHit", true, 1);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.NetworkProperties.CacheHit", 2);
  }

  // Pref should be cleared.
  network_properties_manager.DeleteHistory();
  base::RunLoop().RunUntilIdle();
  {
    TestNetworkPropertiesManager network_properties_manager_2(
        &test_prefs, base::ThreadTaskRunnerHandle::Get());
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_FALSE(network_properties_manager_2.IsCaptivePortal());
    histogram_tester.ExpectBucketCount(
        "DataReductionProxy.NetworkProperties.CacheHit", false, 2);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.NetworkProperties.CacheHit", 3);
  }
}

}  // namespace

}  // namespace data_reduction_proxy