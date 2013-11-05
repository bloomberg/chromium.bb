// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/policy/cloud/cloud_external_data_manager.h"
#include "chrome/browser/policy/cloud/cloud_policy_core.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

const char kExternalDataPath[] = "policy/blank.html";

void ExternalDataFetchCallback(scoped_ptr<std::string>* destination,
                               const base::Closure& done_callback,
                               scoped_ptr<std::string> data) {
  destination->reset(data.release());
  done_callback.Run();
}

}  // namespace

class UserCloudExternalDataManagerTest : public InProcessBrowserTest {
 protected:
  UserCloudExternalDataManagerTest();
  virtual ~UserCloudExternalDataManagerTest();

  scoped_ptr<base::DictionaryValue> ConstructMetadata(const std::string& url,
                                                      const std::string& hash);
  void SetExternalDataReference(const std::string& policy,
                                scoped_ptr<base::DictionaryValue> metadata);

  DISALLOW_COPY_AND_ASSIGN(UserCloudExternalDataManagerTest);
};

UserCloudExternalDataManagerTest::UserCloudExternalDataManagerTest() {
}

UserCloudExternalDataManagerTest::~UserCloudExternalDataManagerTest() {
}

scoped_ptr<base::DictionaryValue>
    UserCloudExternalDataManagerTest::ConstructMetadata(
        const std::string& url,
        const std::string& hash) {
  scoped_ptr<base::DictionaryValue> metadata(new base::DictionaryValue);
  metadata->SetStringWithoutPathExpansion("url", url);
  metadata->SetStringWithoutPathExpansion("hash", base::HexEncode(hash.c_str(),
                                                                  hash.size()));
  return metadata.Pass();
}

void UserCloudExternalDataManagerTest::SetExternalDataReference(
    const std::string& policy,
    scoped_ptr<base::DictionaryValue> metadata) {
#if defined(OS_CHROMEOS)
  UserCloudPolicyManagerChromeOS* policy_manager =
      UserCloudPolicyManagerFactoryChromeOS::GetForProfile(
          browser()->profile());
#else
  UserCloudPolicyManager* policy_manager =
      UserCloudPolicyManagerFactory::GetForProfile(browser()->profile());
#endif
  ASSERT_TRUE(policy_manager);
  CloudPolicyStore* store = policy_manager->core()->store();
  ASSERT_TRUE(store);
  PolicyMap policy_map;
  policy_map.Set(policy,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 metadata.release(),
                 new ExternalDataFetcher(store->external_data_manager(),
                                         policy));
  store->SetPolicyMapForTesting(policy_map);
}

IN_PROC_BROWSER_TEST_F(UserCloudExternalDataManagerTest, FetchExternalData) {
  CloudExternalDataManagerBase::SetMaxExternalDataSizeForTesting(1000);

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  const GURL url =
      embedded_test_server()->GetURL(std::string("/") + kExternalDataPath);

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  std::string external_data;
  ASSERT_TRUE(base::ReadFileToString(test_dir.AppendASCII(kExternalDataPath),
                                     &external_data));
  ASSERT_FALSE(external_data.empty());

  scoped_ptr<base::DictionaryValue> metadata =
      ConstructMetadata(url.spec(), base::SHA1HashString(external_data));
  // TODO(bartfab): The test injects an ExternalDataFetcher for an arbitrary
  // policy. This is only done because there are no policies that reference
  // external data yet. Once the first such policy is added, switch the test to
  // that policy and stop injecting a manually instantiated ExternalDataFetcher.
  SetExternalDataReference(key::kHomepageLocation,
                           make_scoped_ptr(metadata->DeepCopy()));
  content::RunAllPendingInMessageLoop();

  ProfilePolicyConnector* policy_connector =
      ProfilePolicyConnectorFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(policy_connector);
  const PolicyMap& policies = policy_connector->policy_service()->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const PolicyMap::Entry* policy_entry = policies.Get(key::kHomepageLocation);
  ASSERT_TRUE(policy_entry);
  EXPECT_TRUE(base::Value::Equals(metadata.get(), policy_entry->value));
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  base::RunLoop run_loop;
  scoped_ptr<std::string> fetched_external_data;
  policy_entry->external_data_fetcher->Fetch(base::Bind(
      &ExternalDataFetchCallback,
      &fetched_external_data,
      run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(external_data, *fetched_external_data);
}

}  // namespace policy
