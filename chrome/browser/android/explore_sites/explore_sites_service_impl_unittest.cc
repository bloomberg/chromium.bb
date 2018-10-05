// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kCategoryName[] = "Technology";
const char kSite1Url[] = "https://example.com/";
const char kSite2Url[] = "https://sample.com/";
const char kSite1Name[] = "example";
const char kSite2Name[] = "sample";
const char kAcceptLanguages[] = "en-US,en;q=0.5";
}  // namespace

namespace explore_sites {

class ExploreSitesServiceImplTest : public testing::Test {
 public:
  ExploreSitesServiceImplTest();
  ~ExploreSitesServiceImplTest() override = default;

  void SetUp() override {
    std::unique_ptr<ExploreSitesStore> store =
        std::make_unique<ExploreSitesStore>(task_runner_);
    auto history_stats_reporter =
        std::make_unique<HistoryStatisticsReporter>(nullptr, nullptr, nullptr);
    service_ = std::make_unique<ExploreSitesServiceImpl>(
        std::move(store), test_shared_url_loader_factory_,
        std::move(history_stats_reporter));
    success_ = false;
    test_data_ = CreateTestDataProto();
  }

  void UpdateCatalogDoneCallback(bool success) { success_ = success; }
  void CatalogCallback(
      GetCatalogStatus status,
      std::unique_ptr<std::vector<ExploreSitesCategory>> categories) {
    database_status_ = status;
    if (categories != nullptr) {
      database_categories_ = std::move(categories);
    }
  }

  bool success() { return success_; }

  GetCatalogStatus database_status() { return database_status_; }
  std::vector<ExploreSitesCategory>* database_categories() {
    return database_categories_.get();
  }

  ExploreSitesServiceImpl* service() { return service_.get(); }

  std::string test_data() { return test_data_; }

  void PumpLoop() { task_runner_->RunUntilIdle(); }

  std::string CreateTestDataProto();

  ExploreSitesRequestStatus SimulateFetcherData(
      const std::string& response_data);

 private:
  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index);

  std::unique_ptr<explore_sites::ExploreSitesServiceImpl> service_;
  bool success_;
  GetCatalogStatus database_status_;
  std::unique_ptr<std::vector<ExploreSitesCategory>> database_categories_;
  std::string test_data_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::ResourceRequest last_resource_request_;
  base::MessageLoopForIO message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceImplTest);
};

ExploreSitesServiceImplTest::ExploreSitesServiceImplTest()
    : success_(false),
      test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      task_runner_(new base::TestMockTimeTaskRunner) {
  message_loop_.SetTaskRunner(task_runner_);
}

// Called by tests - response_data is the data we want to go back as the
// response from the network.
ExploreSitesRequestStatus ExploreSitesServiceImplTest::SimulateFetcherData(
    const std::string& response_data) {
  DCHECK(test_url_loader_factory_.pending_requests()->size() > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url.spec(), response_data);
  return ExploreSitesRequestStatus::kSuccess;
}

// Helper to check the next request for the network.
network::TestURLLoaderFactory::PendingRequest*
ExploreSitesServiceImplTest::GetPendingRequest(size_t index) {
  if (index >= test_url_loader_factory_.pending_requests()->size())
    return nullptr;
  network::TestURLLoaderFactory::PendingRequest* request =
      &(*test_url_loader_factory_.pending_requests())[index];
  DCHECK(request);
  return request;
}

// This is a helper to generate testing data to use in tests.
std::string ExploreSitesServiceImplTest::CreateTestDataProto() {
  std::string serialized_protobuf;
  explore_sites::GetCatalogResponse catalog_response;
  catalog_response.set_version_token("abcd");
  explore_sites::Catalog* catalog = catalog_response.mutable_catalog();
  explore_sites::Category* category = catalog->add_categories();
  explore_sites::Site* site1 = category->add_sites();
  explore_sites::Site* site2 = category->add_sites();

  // Fill in fields we need to add to the EoS database.

  // Create two sites.
  site1->set_site_url(kSite1Url);
  site1->set_title(kSite1Name);
  site2->set_site_url(kSite2Url);
  site2->set_title(kSite2Name);

  // Create one category, technology.
  category->set_type(Category_CategoryType_TECHNOLOGY);
  category->set_localized_title(kCategoryName);

  // Serialize it into a string.
  catalog_response.SerializeToString(&serialized_protobuf);

  // Print out the string
  DVLOG(1) << "test data proto '" << serialized_protobuf << "'";

  return serialized_protobuf;
}

TEST_F(ExploreSitesServiceImplTest, UpdateCatalogFromNetwork) {
  std::string output_data;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using the test loader factory and test data.
  SimulateFetcherData(test_data());

  // Wait for callback to get called.
  PumpLoop();

  EXPECT_TRUE(success());

  // Get the catalog and verify the contents.

  // First call is to get update_catalog out of the way.  If GetCatalog has
  // never been called before in this session, it won't return anything, it will
  // just start the update process.  For our test, we've already put data into
  // the catalog, but GetCatalog doesn't know that.
  // TODO(petewil): Fix get catalog so it always returns data if it has some.
  service()->GetCatalog(base::BindOnce(
      &ExploreSitesServiceImplTest::CatalogCallback, base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(GetCatalogStatus::kSuccess, database_status());
  EXPECT_NE(nullptr, database_categories());
  EXPECT_EQ(1U, database_categories()->size());

  const ExploreSitesCategory& category = database_categories()->at(0);
  EXPECT_EQ(Category_CategoryType_TECHNOLOGY, category.category_type);
  EXPECT_EQ(std::string(kCategoryName), category.label);
  EXPECT_EQ(2U, category.sites.size());

  // Since the site name and url might come back in a different order than we
  // started with, accept either order as long as one name and url match.
  std::string site1Url = category.sites[0].url.spec();
  std::string site2Url = category.sites[1].url.spec();
  std::string site1Name = category.sites[0].title;
  std::string site2Name = category.sites[1].title;
  EXPECT_TRUE(site1Url == kSite1Url || site1Url == kSite2Url);
  EXPECT_TRUE(site2Url == kSite1Url || site2Url == kSite2Url);
  EXPECT_TRUE(site1Name == kSite1Name || site1Name == kSite2Name);
  EXPECT_TRUE(site2Name == kSite1Name || site2Name == kSite2Name);
}

}  // namespace explore_sites
