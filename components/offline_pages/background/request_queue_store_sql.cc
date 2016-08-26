// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_store_sql.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/save_page_request.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define REQUEST_QUEUE_TABLE_NAME "request_queue_v1"
const bool kUserRequested = true;

bool CreateRequestQueueTable(sql::Connection* db) {
  const char kSql[] = "CREATE TABLE IF NOT EXISTS " REQUEST_QUEUE_TABLE_NAME
                      " (request_id INTEGER PRIMARY KEY NOT NULL,"
                      " creation_time INTEGER NOT NULL,"
                      " activation_time INTEGER NOT NULL DEFAULT 0,"
                      " last_attempt_time INTEGER NOT NULL DEFAULT 0,"
                      " started_attempt_count INTEGER NOT NULL,"
                      " completed_attempt_count INTEGER NOT NULL,"
                      " state INTEGER NOT NULL DEFAULT 0,"
                      " url VARCHAR NOT NULL,"
                      " client_namespace VARCHAR NOT NULL,"
                      " client_id VARCHAR NOT NULL"
                      ")";
  return db->Execute(kSql);
}

bool CreateSchema(sql::Connection* db) {
  // If there is not already a state column, we need to drop the old table.  We
  // are choosing to drop instead of upgrade since the feature is not yet
  // released, so we don't use a transaction to protect existing data, or try to
  // migrate it.
  if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "state")) {
    if (!db->Execute("DROP TABLE IF EXISTS " REQUEST_QUEUE_TABLE_NAME))
      return false;
  }

  if (!CreateRequestQueueTable(db))
    return false;

  // TODO(fgorski): Add indices here.
  return true;
}

// Create a save page request from a SQL result.  Expects complete rows with
// all columns present.  Columns are in order they are defined in select query
// in |RequestQueueStore::RequestSync| method.
SavePageRequest MakeSavePageRequest(const sql::Statement& statement) {
  const int64_t id = statement.ColumnInt64(0);
  const base::Time creation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(1));
  const base::Time activation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(2));
  const base::Time last_attempt_time =
      base::Time::FromInternalValue(statement.ColumnInt64(3));
  const int64_t started_attempt_count = statement.ColumnInt64(4);
  const int64_t completed_attempt_count = statement.ColumnInt64(5);
  const SavePageRequest::RequestState state =
      static_cast<SavePageRequest::RequestState>(statement.ColumnInt64(6));
  const GURL url(statement.ColumnString(7));
  const ClientId client_id(statement.ColumnString(8),
                           statement.ColumnString(9));

  DVLOG(2) << "making save page request - id " << id << " url " << url
           << " client_id " << client_id.name_space << "-" << client_id.id
           << " creation time " << creation_time << " user requested "
           << kUserRequested;

  SavePageRequest request(id, url, client_id, creation_time, activation_time,
                          kUserRequested);
  request.set_last_attempt_time(last_attempt_time);
  request.set_started_attempt_count(started_attempt_count);
  request.set_completed_attempt_count(completed_attempt_count);
  request.set_request_state(state);
  return request;
}

// Get a request for a specific id.
SavePageRequest GetOneRequest(sql::Connection* db, const int64_t request_id) {
  const char kSql[] =
      "SELECT request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id"
      " FROM " REQUEST_QUEUE_TABLE_NAME " WHERE request_id=?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);

  statement.Run();
  return MakeSavePageRequest(statement);
}

void BuildFailedResultList(const std::vector<int64_t>& request_ids,
                           RequestQueue::UpdateMultipleRequestResults results) {
  results.clear();
  for (int64_t request_id : request_ids)
    results.push_back(std::make_pair(
        request_id, RequestQueue::UpdateRequestResult::STORE_FAILURE));
}

RequestQueue::UpdateRequestResult DeleteRequestById(sql::Connection* db,
                                                    int64_t request_id) {
  const char kSql[] =
      "DELETE FROM " REQUEST_QUEUE_TABLE_NAME " WHERE request_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);
  if (!statement.Run())
    return RequestQueue::UpdateRequestResult::STORE_FAILURE;
  else if (db->GetLastChangeCount() == 0)
    return RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
  else
    return RequestQueue::UpdateRequestResult::SUCCESS;
}

bool ChangeRequestState(sql::Connection* db,
                        const int64_t request_id,
                        const SavePageRequest::RequestState new_state) {
  const char kSql[] = "UPDATE " REQUEST_QUEUE_TABLE_NAME
                      " SET state=?"
                      " WHERE request_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, static_cast<int64_t>(new_state));
  statement.BindInt64(1, request_id);
  return statement.Run();
}

bool DeleteRequestsByIds(sql::Connection* db,
                         const std::vector<int64_t>& request_ids,
                         RequestQueue::UpdateMultipleRequestResults& results,
                         std::vector<SavePageRequest>& requests) {
  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    BuildFailedResultList(request_ids, results);
    return false;
  }

  // Read the request before we delete it, and if the delete worked, put it on
  // the queue of requests that got deleted.
  for (int64_t request_id : request_ids) {
    SavePageRequest request = GetOneRequest(db, request_id);
    RequestQueue::UpdateRequestResult result =
        DeleteRequestById(db, request_id);
    results.push_back(std::make_pair(request_id, result));
    if (result == RequestQueue::UpdateRequestResult::SUCCESS)
      requests.push_back(request);
  }

  if (!transaction.Commit()) {
    requests.clear();
    BuildFailedResultList(request_ids, results);
    return false;
  }

  return true;
}

bool ChangeRequestsState(sql::Connection* db,
                         const std::vector<int64_t>& request_ids,
                         SavePageRequest::RequestState new_state,
                         RequestQueue::UpdateMultipleRequestResults& results,
                         std::vector<SavePageRequest>& requests) {
  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    BuildFailedResultList(request_ids, results);
    return false;
  }

  // Update a request, then get it, and put the item we got on the output list.
  for (const auto& request_id : request_ids) {
    RequestQueue::UpdateRequestResult status;
    if (!ChangeRequestState(db, request_id, new_state))
      status = RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    else
      status = RequestQueue::UpdateRequestResult::SUCCESS;

    // Make output request_id/status pair, and put a copy of the updated request
    // on output list.
    results.push_back(std::make_pair(request_id, status));
    requests.push_back(GetOneRequest(db, request_id));
  }

  if (!transaction.Commit()) {
    requests.clear();
    BuildFailedResultList(request_ids, results);
    return false;
  }

  return true;
}

RequestQueueStore::UpdateStatus InsertOrReplace(
    sql::Connection* db,
    const SavePageRequest& request) {
  // In order to use the enums in the Bind* methods, keep the order of fields
  // the same as in the definition/select query.
  const char kInsertSql[] =
      "INSERT OR REPLACE INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time, last_attempt_time, "
      " started_attempt_count, completed_attempt_count, state, url, "
      " client_namespace, client_id) "
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindInt64(0, request.request_id());
  statement.BindInt64(1, request.creation_time().ToInternalValue());
  statement.BindInt64(2, request.activation_time().ToInternalValue());
  statement.BindInt64(3, request.last_attempt_time().ToInternalValue());
  statement.BindInt64(4, request.started_attempt_count());
  statement.BindInt64(5, request.completed_attempt_count());
  statement.BindInt64(6, static_cast<int64_t>(request.request_state()));
  statement.BindString(7, request.url().spec());
  statement.BindString(8, request.client_id().name_space);
  statement.BindString(9, request.client_id().id);

  // TODO(fgorski): Replace the UpdateStatus with boolean in the
  // RequestQueueStore interface and update this code.
  return statement.Run() ? RequestQueueStore::UpdateStatus::UPDATED
                         : RequestQueueStore::UpdateStatus::FAILED;
}

bool InitDatabase(sql::Connection* db, const base::FilePath& path) {
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("BackgroundRequestQueue");
  db->set_exclusive_locking();

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(path.DirName(), &err))
    return false;
  if (!db->Open(path))
    return false;
  db->Preload();

  return CreateSchema(db);
}

}  // anonymous namespace

RequestQueueStoreSQL::RequestQueueStoreSQL(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : background_task_runner_(std::move(background_task_runner)),
      db_file_path_(path.AppendASCII("RequestQueue.db")),
      weak_ptr_factory_(this) {
  OpenConnection();
}

RequestQueueStoreSQL::~RequestQueueStoreSQL() {
  if (db_.get())
    background_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

// static
void RequestQueueStoreSQL::OpenConnectionSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  bool success = InitDatabase(db, path);
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

// static
void RequestQueueStoreSQL::GetRequestsSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const GetRequestsCallback& callback) {
  const char kSql[] =
      "SELECT request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id"
      " FROM " REQUEST_QUEUE_TABLE_NAME;

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  std::vector<SavePageRequest> result;
  while (statement.Step())
    result.push_back(MakeSavePageRequest(statement));

  runner->PostTask(FROM_HERE,
                   base::Bind(callback, statement.Succeeded(), result));
}

// static
void RequestQueueStoreSQL::AddOrUpdateRequestSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const SavePageRequest& request,
    const UpdateCallback& callback) {
  // TODO(fgorski): add UMA metrics here.
  RequestQueueStore::UpdateStatus status = InsertOrReplace(db, request);
  runner->PostTask(FROM_HERE, base::Bind(callback, status));
}

// static
void RequestQueueStoreSQL::RemoveRequestsSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<int64_t>& request_ids,
    const RemoveCallback& callback) {
  RequestQueue::UpdateMultipleRequestResults results;
  std::vector<SavePageRequest> requests;
  // TODO(fgorski): add UMA metrics here.
  DeleteRequestsByIds(db, request_ids, results, requests);
  runner->PostTask(FROM_HERE, base::Bind(callback, results, requests));
}

// static
void RequestQueueStoreSQL::ChangeRequestsStateSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const UpdateMultipleRequestsCallback& callback) {
  RequestQueue::UpdateMultipleRequestResults results;
  std::vector<SavePageRequest> requests;
  // TODO(fgorski): add UMA metrics here.
  offline_pages::ChangeRequestsState(db, request_ids, new_state, results,
                                     requests);
  runner->PostTask(FROM_HERE, base::Bind(callback, results, requests));
}

// static
void RequestQueueStoreSQL::ResetSync(
    sql::Connection* db,
    const base::FilePath& db_file_path,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const ResetCallback& callback) {
  // This method deletes the content of the whole store and reinitializes it.
  bool success = db->Raze();
  db->Close();
  if (success)
    success = InitDatabase(db, db_file_path);
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

bool RequestQueueStoreSQL::CheckDb(const base::Closure& callback) {
  DCHECK(db_.get());
  if (!db_.get()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return false;
  }
  return true;
}

void RequestQueueStoreSQL::GetRequests(const GetRequestsCallback& callback) {
  DCHECK(db_.get());
  if (!CheckDb(base::Bind(callback, false, std::vector<SavePageRequest>())))
    return;

  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RequestQueueStoreSQL::GetRequestsSync, db_.get(),
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void RequestQueueStoreSQL::AddOrUpdateRequest(const SavePageRequest& request,
                                              const UpdateCallback& callback) {
  DCHECK(db_.get());
  if (!CheckDb(base::Bind(callback, UpdateStatus::FAILED)))
    return;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::AddOrUpdateRequestSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request, callback));
}

// RemoveRequestsByRequestId to be more parallell with RemoveRequestsByClientId.
void RequestQueueStoreSQL::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const RemoveCallback& callback) {
  // Set up a failed set of results in case we fail the DB check.
  RequestQueue::UpdateMultipleRequestResults results;
  std::vector<SavePageRequest> requests;
  for (int64_t request_id : request_ids)
    results.push_back(std::make_pair(
        request_id, RequestQueue::UpdateRequestResult::STORE_FAILURE));

  if (!CheckDb(base::Bind(callback, results, requests)))
    return;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::RemoveRequestsSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request_ids, callback));
}

void RequestQueueStoreSQL::ChangeRequestsState(
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const UpdateMultipleRequestsCallback& callback) {
  RequestQueue::UpdateMultipleRequestResults results;
  std::vector<SavePageRequest> requests;
  if (!CheckDb(base::Bind(callback, results, requests)))
    return;

  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RequestQueueStoreSQL::ChangeRequestsStateSync,
                            db_.get(), base::ThreadTaskRunnerHandle::Get(),
                            request_ids, new_state, callback));
}

void RequestQueueStoreSQL::Reset(const ResetCallback& callback) {
  DCHECK(db_.get());
  if (!CheckDb(base::Bind(callback, false)))
    return;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::ResetSync, db_.get(), db_file_path_,
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&RequestQueueStoreSQL::OnResetDone,
                            weak_ptr_factory_.GetWeakPtr(), callback)));
}

void RequestQueueStoreSQL::OpenConnection() {
  DCHECK(!db_);
  db_.reset(new sql::Connection());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::OpenConnectionSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), db_file_path_,
                 base::Bind(&RequestQueueStoreSQL::OnOpenConnectionDone,
                            weak_ptr_factory_.GetWeakPtr())));
}

void RequestQueueStoreSQL::OnOpenConnectionDone(bool success) {
  DCHECK(db_.get());

  // Unfortunately we were not able to open DB connection.
  if (!success)
    db_.reset();
}

void RequestQueueStoreSQL::OnResetDone(const ResetCallback& callback,
                                       bool success) {
  // Complete connection initialization post reset.
  OnOpenConnectionDone(success);
  callback.Run(success);
}

}  // namespace offline_pages
