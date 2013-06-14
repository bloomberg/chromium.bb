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
using content::BrowserThread;

// Receives messages from the backend on the DB thread, posts them to
// WebDatabaseService on the UI thread.
class WebDatabaseService::BackendDelegate :
    public WebDataServiceBackend::Delegate {
 public:
  BackendDelegate(
      const base::WeakPtr<WebDatabaseService>& web_database_service)
      : web_database_service_(web_database_service) {
  }

  virtual void DBLoaded(sql::InitStatus status) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WebDatabaseService::OnDatabaseLoadDone,
                   web_database_service_,
                   status));
  }
 private:
  const base::WeakPtr<WebDatabaseService> web_database_service_;
};

WebDatabaseService::WebDatabaseService(
    const base::FilePath& path,
    const scoped_refptr<base::MessageLoopProxy>& ui_thread)
    : base::RefCountedDeleteOnMessageLoop<WebDatabaseService>(ui_thread),
      path_(path),
      weak_ptr_factory_(this),
      db_loaded_(false) {
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(ui_thread->BelongsToCurrentThread());
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

WebDatabaseService::~WebDatabaseService() {
}

void WebDatabaseService::AddTable(scoped_ptr<WebDatabaseTable> table) {
  if (!wds_backend_.get()) {
    wds_backend_ = new WebDataServiceBackend(
        path_, new BackendDelegate(weak_ptr_factory_.GetWeakPtr()));
  }
  wds_backend_->AddTable(table.Pass());
}

void WebDatabaseService::LoadDatabase() {
  DCHECK(wds_backend_.get());

  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      Bind(&WebDataServiceBackend::InitDatabase, wds_backend_));
}

void WebDatabaseService::UnloadDatabase() {
  db_loaded_ = false;
  if (!wds_backend_.get())
    return;
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      Bind(&WebDataServiceBackend::ShutdownDatabase,
           wds_backend_, true));
}

void WebDatabaseService::ShutdownDatabase() {
  db_loaded_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  loaded_callbacks_.clear();
  error_callbacks_.clear();
  if (!wds_backend_.get())
    return;
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      Bind(&WebDataServiceBackend::ShutdownDatabase,
           wds_backend_, false));
}

WebDatabase* WebDatabaseService::GetDatabaseOnDB() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!wds_backend_.get())
    return NULL;
  return wds_backend_->database();
}

scoped_refptr<WebDataServiceBackend> WebDatabaseService::GetBackend() const {
  return wds_backend_;
}

void WebDatabaseService::ScheduleDBTask(
    const tracked_objects::Location& from_here,
    const WriteTask& task) {
  if (!wds_backend_.get()) {
    NOTREACHED() << "Task scheduled after Shutdown()";
    return;
  }

  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(NULL, wds_backend_->request_manager().get()));

  BrowserThread::PostTask(BrowserThread::DB, from_here,
      Bind(&WebDataServiceBackend::DBWriteTaskWrapper, wds_backend_,
           task, base::Passed(&request)));
}

WebDataServiceBase::Handle WebDatabaseService::ScheduleDBTaskWithResult(
    const tracked_objects::Location& from_here,
    const ReadTask& task,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  WebDataServiceBase::Handle handle = 0;

  if (!wds_backend_.get()) {
    NOTREACHED() << "Task scheduled after Shutdown()";
    return handle;
  }

  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(consumer, wds_backend_->request_manager().get()));
  handle = request->GetHandle();

  BrowserThread::PostTask(BrowserThread::DB, from_here,
      Bind(&WebDataServiceBackend::DBReadTaskWrapper, wds_backend_,
           task, base::Passed(&request)));

  return handle;
}

void WebDatabaseService::CancelRequest(WebDataServiceBase::Handle h) {
  if (!wds_backend_.get())
    return;
  wds_backend_->request_manager()->CancelRequest(h);
}

void WebDatabaseService::RegisterDBLoadedCallback(
    const base::Callback<void(void)>& callback) {
  loaded_callbacks_.push_back(callback);
}

void WebDatabaseService::RegisterDBErrorCallback(
    const base::Callback<void(sql::InitStatus)>& callback) {
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
