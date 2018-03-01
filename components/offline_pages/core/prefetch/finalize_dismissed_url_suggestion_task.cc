// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/finalize_dismissed_url_suggestion_task.h"

#include <memory>

#include "base/bind.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"

namespace offline_pages {
namespace {

bool DeletePageByClientIdIfNotDownloadedSync(const ClientId& client_id,
                                             sql::Connection* db) {
  if (!db)
    return false;
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE client_id = ? AND client_namespace = ? "
      // TODO(carlosk): make this check robust to future changes to the
      // |PrefetchItemState| enum.
      " AND state NOT IN (?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(
      1, static_cast<int>(PrefetchItemErrorCode::SUGGESTION_INVALIDATED));
  statement.BindString(2, client_id.id);
  statement.BindString(3, client_id.name_space);
  statement.BindInt(4, static_cast<int>(PrefetchItemState::DOWNLOADING));
  statement.BindInt(5, static_cast<int>(PrefetchItemState::DOWNLOADED));
  statement.BindInt(6, static_cast<int>(PrefetchItemState::IMPORTING));
  statement.BindInt(7, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(8, static_cast<int>(PrefetchItemState::ZOMBIE));
  return statement.Run();
}
}  // namespace

FinalizeDismissedUrlSuggestionTask::FinalizeDismissedUrlSuggestionTask(
    PrefetchStore* prefetch_store,
    const ClientId& client_id)
    : prefetch_store_(prefetch_store),
      client_id_(client_id),
      weak_ptr_factory_(this) {
  DCHECK(prefetch_store_);
}

FinalizeDismissedUrlSuggestionTask::~FinalizeDismissedUrlSuggestionTask() {}

void FinalizeDismissedUrlSuggestionTask::Run() {
  prefetch_store_->Execute<bool>(
      base::BindOnce(&DeletePageByClientIdIfNotDownloadedSync, client_id_),
      base::BindOnce(&FinalizeDismissedUrlSuggestionTask::OnComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FinalizeDismissedUrlSuggestionTask::OnComplete(bool result) {
  TaskComplete();
}

}  // namespace offline_pages
