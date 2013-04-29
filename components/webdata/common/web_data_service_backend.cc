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
using content::BrowserThread;

WebDataServiceBackend::WebDataServiceBackend(
    const FilePath& path, Delegate* delegate)
    : db_path_(path),
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
       it != tables_.end();
       ++it) {
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

void WebDataServiceBackend::ShutdownDatabase(bool should_reinit) {
  if (db_ && init_status_ == sql::INIT_OK)
    db_->CommitTransaction();
  db_.reset(NULL);
  init_complete_ = !should_reinit; // Setting init_complete_ to true will ensure
  // that the init sequence is not re-run.

  init_status_ = sql::INIT_FAILURE;
}

void WebDataServiceBackend::DBWriteTaskWrapper(
    const WebDatabaseService::WriteTask& task,
    scoped_ptr<WebDataRequest> request) {
  LoadDatabaseIfNecessary();
  if (db_ && init_status_ == sql::INIT_OK && !request->IsCancelled()) {
    WebDatabase::State state = task.Run(db_.get());
    if (state == WebDatabase::COMMIT_NEEDED)
      Commit();
  }
  request_manager_->RequestCompleted(request.Pass());
}

void WebDataServiceBackend::DBReadTaskWrapper(
    const WebDatabaseService::ReadTask& task,
    scoped_ptr<WebDataRequest> request) {
  LoadDatabaseIfNecessary();
  if (db_ && init_status_ == sql::INIT_OK && !request->IsCancelled()) {
    request->SetResult(task.Run(db_.get()).Pass());
  }
  request_manager_->RequestCompleted(request.Pass());
}

WebDataServiceBackend::~WebDataServiceBackend() {
  ShutdownDatabase(false);
}

void WebDataServiceBackend::Commit() {
  if (db_ && init_status_ == sql::INIT_OK) {
    db_->CommitTransaction();
    db_->BeginTransaction();
  } else {
    NOTREACHED() << "Commit scheduled after Shutdown()";
  }
}
