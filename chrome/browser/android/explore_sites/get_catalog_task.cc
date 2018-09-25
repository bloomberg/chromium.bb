// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_catalog_task.h"

#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

static const char kSelectCategorySql[] = R"(SELECT category_id, type, label
FROM categories
WHERE version_token = ?
ORDER BY category_id ASC;)";

static const char kSelectSiteSql[] = R"(SELECT site_id, url, title
FROM sites
WHERE category_id = ?)";

const char kDeleteSiteSql[] = R"(DELETE FROM sites
WHERE category_id NOT IN
(SELECT category_id FROM categories WHERE version_token = ?);)";

const char kDeleteCategorySql[] =
    "DELETE FROM categories WHERE version_token <> ?;";

}  // namespace

std::string UpdateCurrentCatalogIfNewer(sql::MetaTable* meta_table,
                                        std::string current_version_token) {
  DCHECK(meta_table);
  std::string downloading_version_token;
  if (!meta_table->GetValue("downloading_catalog",
                            &downloading_version_token)) {
    return current_version_token;
  }

  if (!meta_table->SetValue("current_catalog", downloading_version_token))
    return "";
  meta_table->DeleteKey("downloading_catalog");

  return downloading_version_token;
}

void RemoveOutdatedCatalogEntries(sql::Database* db,
                                  std::string version_token) {
  sql::Statement delete_sites(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSiteSql));
  delete_sites.BindString(0, version_token);
  delete_sites.Run();

  sql::Statement delete_categories(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteCategorySql));
  delete_categories.BindString(0, version_token);
  delete_categories.Run();
}

std::unique_ptr<GetCatalogTask::CategoryList> GetCatalogSync(
    bool update_current,
    sql::Database* db) {
  DCHECK(db);
  sql::MetaTable meta_table;
  if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
    return nullptr;

  // If we are downloading a catalog that is the same version as the one
  // currently in use, don't change it.  This is an error, should have been
  // caught before we got here.
  std::string catalog_timestamp;
  meta_table.GetValue("current_catalog", &catalog_timestamp);

  if (update_current) {
    sql::Transaction transaction(db);
    transaction.Begin();
    catalog_timestamp =
        UpdateCurrentCatalogIfNewer(&meta_table, catalog_timestamp);
    if (catalog_timestamp == "")
      return nullptr;
    RemoveOutdatedCatalogEntries(db, catalog_timestamp);
    if (!transaction.Commit())
      return nullptr;
  }

  sql::Statement category_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectCategorySql));
  category_statement.BindString(0, catalog_timestamp);

  auto result = std::make_unique<GetCatalogTask::CategoryList>();
  while (category_statement.Step()) {
    result->emplace_back(category_statement.ColumnInt(0),  // category_id
                         catalog_timestamp,
                         category_statement.ColumnInt(1),      // type
                         category_statement.ColumnString(2));  // label
  }
  if (!category_statement.Succeeded())
    return nullptr;

  for (auto& category : *result) {
    sql::Statement site_statement(
        db->GetCachedStatement(SQL_FROM_HERE, kSelectSiteSql));
    site_statement.BindInt64(0, category.category_id);

    while (site_statement.Step()) {
      category.sites.emplace_back(site_statement.ColumnInt(0),  // site_id
                                  category.category_id,
                                  GURL(site_statement.ColumnString(1)),  // url
                                  site_statement.ColumnString(2));  // title
    }
    if (!site_statement.Succeeded())
      return nullptr;
  }

  return result;
}

GetCatalogTask::GetCatalogTask(ExploreSitesStore* store,
                               bool update_current,
                               CatalogCallback callback)
    : store_(store),
      update_current_(update_current),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

GetCatalogTask::~GetCatalogTask() = default;

void GetCatalogTask::Run() {
  store_->Execute(base::BindOnce(&GetCatalogSync, update_current_),
                  base::BindOnce(&GetCatalogTask::FinishedExecuting,
                                 weak_ptr_factory_.GetWeakPtr()),
                  std::unique_ptr<CategoryList>());
}

void GetCatalogTask::FinishedExecuting(
    std::unique_ptr<CategoryList> categories) {
  TaskComplete();
  DVLOG(1) << "Finished getting the catalog, result: " << categories.get();
  std::move(callback_).Run(std::move(categories));
}

}  // namespace explore_sites
