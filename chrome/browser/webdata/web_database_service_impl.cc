// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database_service_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "chrome/browser/api/webdata/web_data_results.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
// TODO(caitkp): Remove this autofill dependency.
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/webdata/web_data_request_manager.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "components/autofill/browser/autofill_country.h"

using base::Bind;
using base::FilePath;
using content::BrowserThread;


////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseServiceInternal implementation.
//
////////////////////////////////////////////////////////////////////////////////

// Refcounted to allow asynchronous destruction on the DB thread.
class WebDatabaseServiceInternal
    : public base::RefCountedThreadSafe<WebDatabaseServiceInternal,
                                        BrowserThread::DeleteOnDBThread> {
 public:

  explicit WebDatabaseServiceInternal(const FilePath& path);

  // Initializes the database and notifies caller via callback when complete.
  void InitDatabaseWithCallback(
      const WebDatabaseService::InitCallback& callback);

  // Opens the database file from the profile path, if an init has not yet been
  // attempted. Separated from the constructor to ease construction/destruction
  // of this object on one thread but database access on the DB thread. Returns
  // the status of the database.
  sql::InitStatus LoadDatabaseIfNecessary();

  void ShutdownDatabase();

  // Task wrappers to run database tasks.
  void DBWriteTaskWrapper(
      const WebDatabaseService::WriteTask& task,
      scoped_ptr<WebDataRequest> request);
  void DBReadTaskWrapper(
      const WebDatabaseService::ReadTask& task,
      scoped_ptr<WebDataRequest> request);

  const scoped_refptr<WebDataRequestManager>& request_manager() {
    return request_manager_; }

  WebDatabase* database() {return db_.get();}

  // Testing.
  void set_init_complete(bool complete) {
    init_complete_ = complete;
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::DB>;
  friend class base::DeleteHelper<WebDatabaseServiceInternal>;

  virtual ~WebDatabaseServiceInternal();

  // Commit the current transaction.
  void Commit();

  // Path to database file.
  FilePath db_path_;

  scoped_ptr<WebDatabase> db_;

  // Keeps track of all pending requests made to the db.
  scoped_refptr<WebDataRequestManager> request_manager_;

  // State of database initialization. Used to prevent the executing of tasks
  // before the db is ready.
  sql::InitStatus init_status_;

  // True if an attempt has been made to load the database (even if the attempt
  // fails), used to avoid continually trying to reinit if the db init fails.
  bool init_complete_;

  // The application locale.  The locale is needed for some database migrations,
  // and must be read on the UI thread.  It's cached here so that we can pass it
  // to the migration code on the DB thread.
  const std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseServiceInternal);
};

////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseServiceInternal implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabaseServiceInternal::WebDatabaseServiceInternal(const FilePath& path)
    : db_path_(path),
      request_manager_(new WebDataRequestManager()),
      init_status_(sql::INIT_FAILURE),
      init_complete_(false),
      app_locale_(AutofillCountry::ApplicationLocale()) {
  db_.reset(NULL);
}

void WebDatabaseServiceInternal::InitDatabaseWithCallback(
    const WebDatabaseService::InitCallback& callback) {
  if (!callback.is_null()) {
    callback.Run(LoadDatabaseIfNecessary());
  }
}

sql::InitStatus WebDatabaseServiceInternal::LoadDatabaseIfNecessary() {
  if (init_complete_ || db_path_.empty()) {
    return init_status_;
  }
  init_complete_ = true;
  db_.reset(new WebDatabase());
  init_status_ = db_->Init(db_path_, app_locale_);
  if (init_status_ != sql::INIT_OK) {
    LOG(ERROR) << "Cannot initialize the web database: " << init_status_;
    db_.reset(NULL);
    return init_status_;
  }

  db_->BeginTransaction();
  return init_status_;
}

void WebDatabaseServiceInternal::ShutdownDatabase() {
  if (db_.get() && init_status_ == sql::INIT_OK)
    db_->CommitTransaction();
  db_.reset(NULL);
  init_complete_ = false;
  init_status_ = sql::INIT_FAILURE;
}

void WebDatabaseServiceInternal::DBWriteTaskWrapper(
    const WebDatabaseService::WriteTask& task,
    scoped_ptr<WebDataRequest> request) {
  LoadDatabaseIfNecessary();
  if (db_.get() && init_status_ == sql::INIT_OK && !request->IsCancelled()) {
    WebDatabase::State state = task.Run(db_.get());
    if (state == WebDatabase::COMMIT_NEEDED)
      Commit();
  }
  request_manager_->RequestCompleted(request.Pass());
}

void WebDatabaseServiceInternal::DBReadTaskWrapper(
    const WebDatabaseService::ReadTask& task,
    scoped_ptr<WebDataRequest> request) {
  LoadDatabaseIfNecessary();
  if (db_.get() && init_status_ == sql::INIT_OK && !request->IsCancelled()) {
    request->SetResult(task.Run(db_.get()).Pass());
  }
  request_manager_->RequestCompleted(request.Pass());
}

WebDatabaseServiceInternal::~WebDatabaseServiceInternal() {
  ShutdownDatabase();
}

void WebDatabaseServiceInternal::Commit() {
  if (db_.get() && init_status_ == sql::INIT_OK) {
      db_->CommitTransaction();
      db_->BeginTransaction();
  } else {
    NOTREACHED() << "Commit scheduled after Shutdown()";
  }
}

////////////////////////////////////////////////////////////////////////////////
WebDatabaseServiceImpl::WebDatabaseServiceImpl(const base::FilePath& path)
    : path_(path) {
  // Do not start WebDatabaseService on ImportProcess.
  DCHECK(!ProfileManager::IsImportProcess(*CommandLine::ForCurrentProcess()));
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

WebDatabaseServiceImpl::~WebDatabaseServiceImpl() {
  wdbs_internal_ = NULL;
}

void WebDatabaseServiceImpl::LoadDatabase(const InitCallback& callback) {
  if (!wdbs_internal_) {
    wdbs_internal_ = new WebDatabaseServiceInternal(path_);
  }
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      Bind(&WebDatabaseServiceInternal::InitDatabaseWithCallback,
           wdbs_internal_, callback));
}

void WebDatabaseServiceImpl::UnloadDatabase() {
  if (wdbs_internal_) {
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        Bind(&WebDatabaseServiceInternal::ShutdownDatabase, wdbs_internal_));
  }
}

WebDatabase* WebDatabaseServiceImpl::GetDatabase() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (wdbs_internal_)
    return wdbs_internal_->database();
  return NULL;
}

void WebDatabaseServiceImpl::ScheduleDBTask(
      const tracked_objects::Location& from_here,
      const WriteTask& task) {
  if (!wdbs_internal_) {
    NOTREACHED() << "Task scheduled after Shutdown()";
    return;
  }

  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(NULL, wdbs_internal_->request_manager()));

  BrowserThread::PostTask(BrowserThread::DB, from_here,
      Bind(&WebDatabaseServiceInternal::DBWriteTaskWrapper, wdbs_internal_,
           task, base::Passed(&request)));
}

WebDataService::Handle WebDatabaseServiceImpl::ScheduleDBTaskWithResult(
      const tracked_objects::Location& from_here,
      const ReadTask& task,
      WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  WebDataService::Handle handle = 0;

  if (!wdbs_internal_) {
    NOTREACHED() << "Task scheduled after Shutdown()";
    return handle;
  }

  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(consumer, wdbs_internal_->request_manager()));
  handle = request->GetHandle();

  BrowserThread::PostTask(BrowserThread::DB, from_here,
      Bind(&WebDatabaseServiceInternal::DBReadTaskWrapper, wdbs_internal_,
           task, base::Passed(&request)));

  return handle;
}

void WebDatabaseServiceImpl::CancelRequest(WebDataServiceBase::Handle h) {
  if (wdbs_internal_)
    wdbs_internal_->request_manager()->CancelRequest(h);
}

// Testing
void WebDatabaseServiceImpl::set_init_complete(bool complete) {
  if (wdbs_internal_)
    wdbs_internal_->set_init_complete(complete);
}
