// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_catalog_task.h"

#include <memory>

#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/task/task.h"
#include "components/offline_pages/task/task_test_base.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_pages::TaskTestBase;

namespace explore_sites {

class ExploreSitesGetCatalogTaskTest : public TaskTestBase {
 public:
  ExploreSitesGetCatalogTaskTest() = default;
  ~ExploreSitesGetCatalogTaskTest() override = default;

  void SetUp() override {
    store_ = std::make_unique<ExploreSitesStore>(task_runner());
  }

  ExploreSitesStore* store() { return store_.get(); }

  void ExecuteSync(base::RepeatingCallback<bool(sql::Database*)> query) {
    store()->Execute(base::OnceCallback<bool(sql::Database*)>(query),
                     base::BindOnce([](bool result) { ASSERT_TRUE(result); }),
                     false);
    RunUntilIdle();
  }

  void PopulateTestingCatalog();
  void ValidateTestingCatalog(GetCatalogTask::CategoryList* catalog);
  void SetDownloadingAndCurrentVersion(int downloading_version,
                                       int current_version);
  std::pair<int, int> GetCurrentAndDownloadingVersion();
  int GetNumberOfCategoriesInDB();
  int GetNumberOfSitesInDB();

 private:
  std::unique_ptr<ExploreSitesStore> store_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesGetCatalogTaskTest);
};

void ExploreSitesGetCatalogTaskTest::PopulateTestingCatalog() {
  // Populate a catalog with test data.  There are two out-dated categories and
  // two current categories.  Each category (of the 4) has a site, but only the
  // current categories should be returned.
  ExecuteSync(base::BindLambdaForTesting([](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.SetValue("current_catalog", 5678);
    meta_table.DeleteKey("downloading_catalog");
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO categories
(category_id, version, type, label)
VALUES
(1, 1234, 1, "label_1"), -- older catalog
(2, 1234, 2, "label_2"), -- older catalog
(3, 5678, 1, "label_1"), -- current catalog
(4, 5678, 2, "label_2"); -- current catalog)"));
    if (!insert.Run())
      return false;

    sql::Statement insert_sites(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title)
VALUES
(1, "https://www.example.com/1", 1, "example_old_1"),
(2, "https://www.example.com/2", 2, "example_old_2"),
(3, "https://www.example.com/1", 3, "example_1"),
(4, "https://www.example.com/2", 4, "example_2");
    )"));
    return insert_sites.Run();
  }));
}

void ExploreSitesGetCatalogTaskTest::ValidateTestingCatalog(
    GetCatalogTask::CategoryList* catalog) {
  EXPECT_FALSE(catalog == nullptr);

  EXPECT_EQ(2U, catalog->size());
  ExploreSitesCategory* cat = &catalog->at(0);
  EXPECT_EQ(3, cat->category_id);
  EXPECT_EQ(5678, cat->version);
  EXPECT_EQ(1, cat->category_type);
  EXPECT_EQ("label_1", cat->label);

  EXPECT_EQ(1U, cat->sites.size());
  ExploreSitesSite* site = &cat->sites[0];
  EXPECT_EQ("https://www.example.com/1", site->url.spec());
  EXPECT_EQ(3, site->category_id);
  EXPECT_EQ("example_1", site->title);

  cat = &catalog->at(1);
  EXPECT_EQ(4, cat->category_id);
  EXPECT_EQ(5678, cat->version);
  EXPECT_EQ(2, cat->category_type);
  EXPECT_EQ("label_2", cat->label);

  EXPECT_EQ(1U, cat->sites.size());
  site = &cat->sites[0];
  EXPECT_EQ("https://www.example.com/2", site->url.spec());
  EXPECT_EQ(4, site->category_id);
  EXPECT_EQ("example_2", site->title);
}

void ExploreSitesGetCatalogTaskTest::SetDownloadingAndCurrentVersion(
    int downloading_version,
    int current_version) {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.SetValue("downloading_catalog", downloading_version);
    meta_table.SetValue("current_catalog", current_version);
    return true;
  }));
}

std::pair<int, int>
ExploreSitesGetCatalogTaskTest::GetCurrentAndDownloadingVersion() {
  int64_t current_catalog = -1;
  int64_t downloading_catalog = -1;
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.GetValue("current_catalog", &current_catalog);
    meta_table.GetValue("downloading_catalog", &downloading_catalog);
    return true;
  }));
  return std::make_pair(current_catalog, downloading_catalog);
}

int ExploreSitesGetCatalogTaskTest::GetNumberOfCategoriesInDB() {
  int result = -1;
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_count(
        db->GetUniqueStatement("SELECT COUNT(*) FROM categories"));
    cat_count.Step();
    result = cat_count.ColumnInt(0);
    return true;
  }));
  return result;
}

int ExploreSitesGetCatalogTaskTest::GetNumberOfSitesInDB() {
  int result = -1;
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement site_count(
        db->GetUniqueStatement("SELECT COUNT(*) FROM sites"));
    site_count.Step();
    result = site_count.ColumnInt(0);
    return true;
  }));
  return result;
}

TEST_F(ExploreSitesGetCatalogTaskTest, StoreFailure) {
  store()->SetInitializationStatusForTest(InitializationStatus::FAILURE);

  auto callback = base::BindLambdaForTesting(
      [](std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        EXPECT_TRUE(catalog == nullptr);
      });
  GetCatalogTask task(store(), false, callback);
  RunTask(&task);
}

TEST_F(ExploreSitesGetCatalogTaskTest, EmptyTask) {
  auto callback = base::BindLambdaForTesting(
      [](std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        EXPECT_FALSE(catalog == nullptr);
      });
  GetCatalogTask task(store(), false, callback);
  RunTask(&task);
}

// This tests the behavior of the catalog task when there is already a catalog
// with the current timestamp in the database. This tests both the case where it
// is the "current" catalog, and where it is the "downloading" catalog.
TEST_F(ExploreSitesGetCatalogTaskTest, SimpleCatalog) {
  PopulateTestingCatalog();
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        ValidateTestingCatalog(catalog.get());
      });
  GetCatalogTask task(store(), false, callback);
  RunTask(&task);
  // Since |update_current| is false, we should not have changed any rows in the
  // DB.
  EXPECT_EQ(4, GetNumberOfCategoriesInDB());
  EXPECT_EQ(4, GetNumberOfSitesInDB());
}

TEST_F(ExploreSitesGetCatalogTaskTest, CatalogWithVersionUpdate) {
  PopulateTestingCatalog();
  // Update the testing catalog so that the older catalog is current and the
  // downloading catalog is ready to upgrade.
  SetDownloadingAndCurrentVersion(5678, 1234);
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        ValidateTestingCatalog(catalog.get());
      });
  GetCatalogTask task(store(), true /* update_current */, callback);
  RunTask(&task);

  EXPECT_EQ(std::make_pair(5678, -1), GetCurrentAndDownloadingVersion());
  // The task should have pruned the database.
  EXPECT_EQ(2, GetNumberOfCategoriesInDB());
  EXPECT_EQ(2, GetNumberOfSitesInDB());
}

TEST_F(ExploreSitesGetCatalogTaskTest, CatalogWithoutVersionUpdate) {
  PopulateTestingCatalog();
  // Make "1234" the downloading version, we should not see any changes in the
  // DB if the |update_current| flag is false.
  SetDownloadingAndCurrentVersion(1234, 5678);
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        ValidateTestingCatalog(catalog.get());
      });
  GetCatalogTask task(store(), false /* update_current */, callback);
  RunTask(&task);

  EXPECT_EQ(std::make_pair(5678, 1234), GetCurrentAndDownloadingVersion());
  EXPECT_EQ(4, GetNumberOfCategoriesInDB());
  EXPECT_EQ(4, GetNumberOfSitesInDB());
}

TEST_F(ExploreSitesGetCatalogTaskTest, InvalidCatalogVersions) {
  PopulateTestingCatalog();
  SetDownloadingAndCurrentVersion(-1, -1);
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        EXPECT_EQ(0U, catalog->size());
      });
  GetCatalogTask task(store(), false /* update_current */, callback);
  RunTask(&task);
}

}  // namespace explore_sites
