// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_backend.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"

using base::Bind;
using base::FilePath;

WebDataServiceBackend::WebDataServiceBackend(
    const FilePath& path,
    Delegate* delegate,
    const scoped_refptr<base::MessageLoopProxy>& db_thread)
    : base::RefCountedDeleteOnMessageLoop<WebDataServiceBackend>(db_thread),
      db_path_(path),
      request_manager_(new WebDataRequestManager()),
      init_status_(sql::INIT_FAILURE),
      init_complete_(false),
      delegate_(delegate) {
}

void WebDataServiceBackend::AddTable(scoped_ptr<WebDatabaseTable> table) {
  DCHECK(!db_.get());
  tables_.push_back(table.release());
}

void WebDataServiceBackend::InitDatabase() {
  LoadDatabaseIfNecessary();
  if (delegate_) {
    delegate_->DBLoaded(init_status_);
  }
}

sql::InitStatus WebDataServiceBackend::LoadDatabaseIfNecessary() {
  if (init_complete_ || db_path_.empty()) {
    return init_status_;
  }
  init_complete_ = true;
  db_.reset(new WebDatabase());

  for (ScopedVector<WebDatabaseTable>::iterator it = tables_.begin();
       it != tables_.end(); ++it) {
    db_->AddTable(*it);
  }

  init_status_ = db_->Init(db_path_);
  if (init_status_ != sql::INIT_OK) {
    LOG(ERROR) << "Cannot initialize the web database: " << init_status_;
    db_.reset(NULL);
    return init_status_;
  }

  db_->BeginTransaction();
  return init_status_;
}

void WebDataServiceBackend::ShutdownDatabase() {
  if (db_ && init_status_ == sql::INIT_OK)
    db_->CommitTransaction();
  db_.reset(NULL);
  init_complete_ = true;  // Ensures the init sequence is not re-run.
  init_status_ = sql::INIT_FAILURE;
}

void WebDataServiceBackend::DBWriteTaskWrapper(
    const WebDatabaseService::WriteTask& task,
    scoped_ptr<WebDataRequest> request) {
  if (request->IsCancelled())
    return;

  ExecuteWriteTask(task);
  request_manager_->RequestCompleted(request.Pass());
}

void WebDataServiceBackend::ExecuteWriteTask(
    const WebDatabaseService::WriteTask& task) {
  LoadDatabaseIfNecessary();
  if (db_ && init_status_ == sql::INIT_OK) {
    WebDatabase::State state = task.Run(db_.get());
    if (state == WebDatabase::COMMIT_NEEDED)
      Commit();
  }
}

void WebDataServiceBackend::DBReadTaskWrapper(
    const WebDatabaseService::ReadTask& task,
    scoped_ptr<WebDataRequest> request) {
  if (request->IsCancelled())
    return;

  request->SetResult(ExecuteReadTask(task).Pass());
  request_manager_->RequestCompleted(request.Pass());
}

scoped_ptr<WDTypedResult> WebDataServiceBackend::ExecuteReadTask(
    const WebDatabaseService::ReadTask& task) {
  LoadDatabaseIfNecessary();
  if (db_ && init_status_ == sql::INIT_OK) {
    return task.Run(db_.get());
  }
  return scoped_ptr<WDTypedResult>();
}

WebDataServiceBackend::~WebDataServiceBackend() {
  ShutdownDatabase();
}

void WebDataServiceBackend::Commit() {
  DCHECK(db_);
  DCHECK_EQ(sql::INIT_OK, init_status_);
  db_->CommitTransaction();
  db_->BeginTransaction();
}
