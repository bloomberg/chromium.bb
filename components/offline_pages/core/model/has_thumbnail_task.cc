// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/has_thumbnail_task.h"

#include "base/bind.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {
VisualsAvailability ThumbnailExistsSync(int64_t offline_id, sql::Database* db) {
  static const char kSql[] =
      "SELECT length(thumbnail)>0,length(favicon)>0 FROM page_thumbnails"
      " WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  if (!statement.Step())
    return {false, false};

  return {statement.ColumnBool(0), statement.ColumnBool(1)};
}

}  // namespace

HasThumbnailTask::HasThumbnailTask(OfflinePageMetadataStore* store,
                                   int64_t offline_id,
                                   ThumbnailExistsCallback exists_callback)
    : store_(store),
      offline_id_(offline_id),
      exists_callback_(std::move(exists_callback)),
      weak_ptr_factory_(this) {}

HasThumbnailTask::~HasThumbnailTask() = default;

void HasThumbnailTask::Run() {
  store_->Execute(base::BindOnce(ThumbnailExistsSync, offline_id_),
                  base::BindOnce(&HasThumbnailTask::OnThumbnailExists,
                                 weak_ptr_factory_.GetWeakPtr()),
                  {false, false});
}

void HasThumbnailTask::OnThumbnailExists(VisualsAvailability availability) {
  TaskComplete();
  std::move(exists_callback_).Run(std::move(availability));
}

}  // namespace offline_pages
