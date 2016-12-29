// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

const char kExternalDataPath[] = "policy/blank.html";

}  // namespace

typedef InProcessBrowserTest UserCloudExternalDataManagerTest;

IN_PROC_BROWSER_TEST_F(UserCloudExternalDataManagerTest, FetchExternalData) {
  CloudExternalDataManagerBase::SetMaxExternalDataSizeForTesting(1000);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url =
      embedded_test_server()->GetURL(std::string("/") + kExternalDataPath);

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  std::string external_data;
  ASSERT_TRUE(base::ReadFileToString(test_dir.AppendASCII(kExternalDataPath),
                                     &external_data));
  ASSERT_FALSE(external_data.empty());

  std::unique_ptr<base::DictionaryValue> metadata =
      test::ConstructExternalDataReference(url.spec(), external_data);
#if defined(OS_CHROMEOS)
  UserCloudPolicyManagerChromeOS* policy_manager =
      UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          browser()->profile());
#else
  UserCloudPolicyManager* policy_manager =
      UserCloudPolicyManagerFactory::GetForBrowserContext(browser()->profile());
#endif
  ASSERT_TRUE(policy_manager);
  // TODO(bartfab): The test injects an ExternalDataFetcher for an arbitrary
  // policy. This is only done because there are no policies that reference
  // external data yet. Once the first such policy is added, switch the test to
  // that policy and stop injecting a manually instantiated ExternalDataFetcher.
  test::SetExternalDataReference(policy_manager->core(), key::kHomepageLocation,
                                 base::WrapUnique(metadata->DeepCopy()));
  content::RunAllPendingInMessageLoop();

  ProfilePolicyConnector* policy_connector =
      ProfilePolicyConnectorFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(policy_connector);
  const PolicyMap& policies = policy_connector->policy_service()->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const PolicyMap::Entry* policy_entry = policies.Get(key::kHomepageLocation);
  ASSERT_TRUE(policy_entry);
  EXPECT_TRUE(base::Value::Equals(metadata.get(), policy_entry->value.get()));
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  base::RunLoop run_loop;
  std::unique_ptr<std::string> fetched_external_data;
  policy_entry->external_data_fetcher->Fetch(base::Bind(
      &test::ExternalDataFetchCallback,
      &fetched_external_data,
      run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(external_data, *fetched_external_data);
}

}  // namespace policy
