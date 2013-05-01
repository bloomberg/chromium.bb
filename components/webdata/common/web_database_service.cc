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
    const base::FilePath& path)
    : path_(path),
      weak_ptr_factory_(this) {
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

WebDatabaseService::~WebDatabaseService() {
}

void WebDatabaseService::AddTable(scoped_ptr<WebDatabaseTable> table) {
  if (!wds_backend_) {
    wds_backend_ = new WebDataServiceBackend(
        path_, new BackendDelegate(weak_ptr_factory_.GetWeakPtr()));
  }
  wds_backend_->AddTable(table.Pass());
}

void WebDatabaseService::LoadDatabase() {
  DCHECK(wds_backend_);

  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      Bind(&WebDataServiceBackend::InitDatabase, wds_backend_));
}

void WebDatabaseService::UnloadDatabase() {
  if (!wds_backend_)
    return;
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      Bind(&WebDataServiceBackend::ShutdownDatabase,
           wds_backend_, true));
}

void WebDatabaseService::ShutdownDatabase() {
  if (!wds_backend_)
    return;
  weak_ptr_factory_.InvalidateWeakPtrs();
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      Bind(&WebDataServiceBackend::ShutdownDatabase,
           wds_backend_, false));
}

WebDatabase* WebDatabaseService::GetDatabaseOnDB() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!wds_backend_)
    return NULL;
  return wds_backend_->database();
}

void WebDatabaseService::ScheduleDBTask(
    const tracked_objects::Location& from_here,
    const WriteTask& task) {
  if (!wds_backend_) {
    NOTREACHED() << "Task scheduled after Shutdown()";
    return;
  }

  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(NULL, wds_backend_->request_manager()));

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

  if (!wds_backend_) {
    NOTREACHED() << "Task scheduled after Shutdown()";
    return handle;
  }

  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(consumer, wds_backend_->request_manager()));
  handle = request->GetHandle();

  BrowserThread::PostTask(BrowserThread::DB, from_here,
      Bind(&WebDataServiceBackend::DBReadTaskWrapper, wds_backend_,
           task, base::Passed(&request)));

  return handle;
}

void WebDatabaseService::CancelRequest(WebDataServiceBase::Handle h) {
  if (!wds_backend_)
    return;
  wds_backend_->request_manager()->CancelRequest(h);
}

void WebDatabaseService::AddObserver(WebDatabaseObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WebDatabaseService::RemoveObserver(WebDatabaseObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WebDatabaseService::OnDatabaseLoadDone(sql::InitStatus status) {
  if (status == sql::INIT_OK) {
    // Notify that the database has been initialized.
    FOR_EACH_OBSERVER(WebDatabaseObserver,
                      observer_list_,
                      WebDatabaseLoaded());
  } else {
    // Notify that the database load failed.
    FOR_EACH_OBSERVER(WebDatabaseObserver,
                      observer_list_,
                      WebDatabaseLoadFailed(status));
  }
}
