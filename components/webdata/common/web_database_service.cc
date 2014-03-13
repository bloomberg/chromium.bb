// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_backend.h"
#include "components/webdata/common/web_data_service_consumer.h"

using base::Bind;
using base::FilePath;

// Receives messages from the backend on the DB thread, posts them to
// WebDatabaseService on the UI thread.
class WebDatabaseService::BackendDelegate :
    public WebDataServiceBackend::Delegate {
 public:
  BackendDelegate(
      const base::WeakPtr<WebDatabaseService>& web_database_service)
      : web_database_service_(web_database_service),
        callback_thread_(base::MessageLoopProxy::current()) {
  }

  virtual void DBLoaded(sql::InitStatus status) OVERRIDE {
    callback_thread_->PostTask(
        FROM_HERE,
        base::Bind(&WebDatabaseService::OnDatabaseLoadDone,
                   web_database_service_,
                   status));
  }
 private:
  const base::WeakPtr<WebDatabaseService> web_database_service_;
  scoped_refptr<base::MessageLoopProxy> callback_thread_;
};

WebDatabaseService::WebDatabaseService(
    const base::FilePath& path,
    const scoped_refptr<base::MessageLoopProxy>& ui_thread,
    const scoped_refptr<base::MessageLoopProxy>& db_thread)
    : base::RefCountedDeleteOnMessageLoop<WebDatabaseService>(ui_thread),
      path_(path),
      db_loaded_(false),
      db_thread_(db_thread),
      weak_ptr_factory_(this) {
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(ui_thread->BelongsToCurrentThread());
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(db_thread.get());
}

WebDatabaseService::~WebDatabaseService() {
}

void WebDatabaseService::AddTable(scoped_ptr<WebDatabaseTable> table) {
  if (!wds_backend_) {
    wds_backend_ = new WebDataServiceBackend(
        path_, new BackendDelegate(weak_ptr_factory_.GetWeakPtr()),
        db_thread_);
  }
  wds_backend_->AddTable(table.Pass());
}

void WebDatabaseService::LoadDatabase() {
  DCHECK(wds_backend_);
  db_thread_->PostTask(
      FROM_HERE,
      Bind(&WebDataServiceBackend::InitDatabase, wds_backend_));
}

void WebDatabaseService::ShutdownDatabase() {
  db_loaded_ = false;
  loaded_callbacks_.clear();
  error_callbacks_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!wds_backend_)
    return;
  db_thread_->PostTask(
      FROM_HERE, Bind(&WebDataServiceBackend::ShutdownDatabase, wds_backend_));
}

WebDatabase* WebDatabaseService::GetDatabaseOnDB() const {
  DCHECK(db_thread_->BelongsToCurrentThread());
  return wds_backend_ ? wds_backend_->database() : NULL;
}

scoped_refptr<WebDataServiceBackend> WebDatabaseService::GetBackend() const {
  return wds_backend_;
}

void WebDatabaseService::ScheduleDBTask(
    const tracked_objects::Location& from_here,
    const WriteTask& task) {
  DCHECK(wds_backend_);
  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(NULL, wds_backend_->request_manager().get()));
  db_thread_->PostTask(from_here,
                       Bind(&WebDataServiceBackend::DBWriteTaskWrapper,
                            wds_backend_, task, base::Passed(&request)));
}

WebDataServiceBase::Handle WebDatabaseService::ScheduleDBTaskWithResult(
    const tracked_objects::Location& from_here,
    const ReadTask& task,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_backend_);
  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(consumer, wds_backend_->request_manager().get()));
  WebDataServiceBase::Handle handle = request->GetHandle();
  db_thread_->PostTask(from_here,
                       Bind(&WebDataServiceBackend::DBReadTaskWrapper,
                            wds_backend_, task, base::Passed(&request)));
  return handle;
}

void WebDatabaseService::CancelRequest(WebDataServiceBase::Handle h) {
  if (!wds_backend_)
    return;
  wds_backend_->request_manager()->CancelRequest(h);
}

void WebDatabaseService::RegisterDBLoadedCallback(
    const DBLoadedCallback& callback) {
  loaded_callbacks_.push_back(callback);
}

void WebDatabaseService::RegisterDBErrorCallback(
    const DBLoadErrorCallback& callback) {
  error_callbacks_.push_back(callback);
}

void WebDatabaseService::OnDatabaseLoadDone(sql::InitStatus status) {
  if (status == sql::INIT_OK) {
    db_loaded_ = true;

    for (size_t i = 0; i < loaded_callbacks_.size(); i++) {
      if (!loaded_callbacks_[i].is_null())
        loaded_callbacks_[i].Run();
    }

    loaded_callbacks_.clear();
  } else {
    // Notify that the database load failed.
    for (size_t i = 0; i < error_callbacks_.size(); i++) {
      if (!error_callbacks_[i].is_null())
        error_callbacks_[i].Run(status);
    }

    error_callbacks_.clear();
  }
}
