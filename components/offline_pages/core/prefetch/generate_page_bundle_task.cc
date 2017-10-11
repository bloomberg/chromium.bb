// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_task.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/time/default_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

// Temporary storage for Urls metadata fetched from the storage.
struct FetchedUrl {
  FetchedUrl() = default;
  FetchedUrl(int64_t offline_id,
             const std::string& requested_url,
             int generate_bundle_attempts)
      : offline_id(offline_id),
        requested_url(requested_url),
        generate_bundle_attempts(generate_bundle_attempts) {}

  int64_t offline_id;
  std::string requested_url;
  int generate_bundle_attempts;
};

// This is maximum URLs that Offline Page Service can take in one request.
const int kMaxUrlsToSend = 100;

bool UpdateStateSync(sql::Connection* db,
                     const int64_t offline_id,
                     base::Clock* clock) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?,"
      "     freshness_time = CASE WHEN generate_bundle_attempts = 0 THEN ?"
      "                           ELSE freshness_time END,"
      "     generate_bundle_attempts = generate_bundle_attempts + 1"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(
      0, static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE));
  statement.BindInt64(1, store_utils::ToDatabaseTime(clock->Now()));
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

std::unique_ptr<std::vector<FetchedUrl>> FetchUrlsSync(sql::Connection* db) {
  static const char kSql[] =
      "SELECT offline_id, requested_url, generate_bundle_attempts"
      " FROM prefetch_items"
      " WHERE state = ?"
      " ORDER BY creation_time DESC";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::NEW_REQUEST));

  auto urls = base::MakeUnique<std::vector<FetchedUrl>>();
  while (statement.Step()) {
    urls->push_back(
        FetchedUrl(statement.ColumnInt64(0),   // offline_id
                   statement.ColumnString(1),  // requested_url
                   statement.ColumnInt(2)));   // generate_bundle_attempts
  }

  return urls;
}

bool MarkUrlFinishedWithError(sql::Connection* db, const FetchedUrl& url) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1,
                    static_cast<int>(PrefetchItemErrorCode::TOO_MANY_NEW_URLS));
  statement.BindInt64(2, url.offline_id);
  return statement.Run();
}

std::unique_ptr<std::vector<std::string>> SelectUrlsToPrefetchSync(
    base::Clock* clock,
    sql::Connection* db) {
  if (!db)
    return nullptr;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return nullptr;

  auto urls = FetchUrlsSync(db);
  if (!urls || urls->empty())
    return nullptr;

  // If we've got more than kMaxUrlsToSend URLs, mark the extra ones FINISHED
  // and remove them from the list.
  if (urls->size() > kMaxUrlsToSend) {
    for (size_t index = kMaxUrlsToSend; index < urls->size(); ++index) {
      if (!MarkUrlFinishedWithError(db, urls->at(index)))
        return nullptr;
    }
    urls->resize(kMaxUrlsToSend);
  }

  auto url_specs = base::MakeUnique<std::vector<std::string>>();
  for (const auto& url : *urls) {
    if (!UpdateStateSync(db, url.offline_id, clock))
      return nullptr;
    url_specs->push_back(std::move(url.requested_url));
  }

  if (!transaction.Commit())
    return nullptr;

  return url_specs;
}
}  // namespace

GeneratePageBundleTask::GeneratePageBundleTask(
    PrefetchStore* prefetch_store,
    PrefetchGCMHandler* gcm_handler,
    PrefetchNetworkRequestFactory* request_factory,
    const PrefetchRequestFinishedCallback& callback)
    : clock_(new base::DefaultClock()),
      prefetch_store_(prefetch_store),
      gcm_handler_(gcm_handler),
      request_factory_(request_factory),
      callback_(callback),
      weak_factory_(this) {}

GeneratePageBundleTask::~GeneratePageBundleTask() {}

void GeneratePageBundleTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&SelectUrlsToPrefetchSync, clock_.get()),
      base::BindOnce(&GeneratePageBundleTask::StartGeneratePageBundle,
                     weak_factory_.GetWeakPtr()));
}

void GeneratePageBundleTask::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void GeneratePageBundleTask::StartGeneratePageBundle(
    std::unique_ptr<std::vector<std::string>> urls) {
  if (!urls || urls->empty()) {
    TaskComplete();
    return;
  }

  gcm_handler_->GetGCMToken(
      base::Bind(&GeneratePageBundleTask::GotRegistrationId,
                 weak_factory_.GetWeakPtr(), base::Passed(std::move(urls))));
}

void GeneratePageBundleTask::GotRegistrationId(
    std::unique_ptr<std::vector<std::string>> urls,
    const std::string& id,
    instance_id::InstanceID::Result result) {
  // TODO(dimich): Add UMA reporting on instance_id::InstanceID::Result.
  request_factory_->MakeGeneratePageBundleRequest(*urls, id, callback_);
  TaskComplete();
}

}  // namespace offline_pages
