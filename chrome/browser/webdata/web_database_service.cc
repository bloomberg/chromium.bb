// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/api/webdata/web_data_results.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
#include "chrome/browser/webdata/web_data_request_manager.h"

using base::Bind;
using base::FilePath;
using content::BrowserThread;


////////////////////////////////////////////////////////////////////////////////
//
// WebDataServiceBackend implementation.
//
////////////////////////////////////////////////////////////////////////////////

// Refcounted to allow asynchronous destruction on the DB thread.
class WebDataServiceBackend
    : public base::RefCountedThreadSafe<WebDataServiceBackend,
                                        BrowserThread::DeleteOnDBThread> {
 public:
  explicit WebDataServiceBackend(const FilePath& path);

  // Must call only before InitDatabaseWithCallback.
  void AddTable(scoped_ptr<WebDatabaseTable> table);

  // Initializes the database and notifies caller via callback when complete.
  // Callback is called synchronously.
  void InitDatabaseWithCallback(
      const WebDatabaseService::InitCallback& callback);

  // Opens the database file from the profile path if an init has not yet been
  // attempted. Separated from the constructor to ease construction/destruction
  // of this object on one thread but database access on the DB thread. Returns
  // the status of the database.
  sql::InitStatus LoadDatabaseIfNecessary();

  // Shuts down database. |should_reinit| tells us whether or not it should be
  // possible to re-initialize the DB after the shutdown.
  void ShutdownDatabase(bool should_reinit);

  // Task wrappers to run database tasks.
  void DBWriteTaskWrapper(
      const WebDatabaseService::WriteTask& task,
      scoped_ptr<WebDataRequest> request);
  void DBReadTaskWrapper(
      const WebDatabaseService::ReadTask& task,
      scoped_ptr<WebDataRequest> request);

  const scoped_refptr<WebDataRequestManager>& request_manager() {
    return request_manager_;
  }

  WebDatabase* database() { return db_.get(); }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::DB>;
  friend class base::DeleteHelper<WebDataServiceBackend>;

  virtual ~WebDataServiceBackend();

  // Commit the current transaction.
  void Commit();

  // Path to database file.
  FilePath db_path_;

  // The tables that participate in managing the database. These are
  // owned here but other than that this class does nothing with
  // them. Their initialization is in whatever factory creates
  // WebDatabaseService, and lookup by type is provided by the
  // WebDatabase class. The tables need to be owned by this refcounted
  // object, or they themselves would need to be refcounted. Owning
  // them here rather than having WebDatabase own them makes for
  // easier unit testing of WebDatabase.
  ScopedVector<WebDatabaseTable> tables_;

  scoped_ptr<WebDatabase> db_;

  // Keeps track of all pending requests made to the db.
  scoped_refptr<WebDataRequestManager> request_manager_;

  // State of database initialization. Used to prevent the executing of tasks
  // before the db is ready.
  sql::InitStatus init_status_;

  // True if an attempt has been made to load the database (even if the attempt
  // fails), used to avoid continually trying to reinit if the db init fails.
  bool init_complete_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceBackend);
};

WebDataServiceBackend::WebDataServiceBackend(
    const FilePath& path)
    : db_path_(path),
      request_manager_(new WebDataRequestManager()),
      init_status_(sql::INIT_FAILURE),
      init_complete_(false) {
}

void WebDataServiceBackend::AddTable(scoped_ptr<WebDatabaseTable> table) {
  DCHECK(!db_.get());
  tables_.push_back(table.release());
}

void WebDataServiceBackend::InitDatabaseWithCallback(
    const WebDatabaseService::InitCallback& callback) {
  if (!callback.is_null()) {
    callback.Run(LoadDatabaseIfNecessary());
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

////////////////////////////////////////////////////////////////////////////////
WebDatabaseService::WebDatabaseService(
    const base::FilePath& path)
    : path_(path) {
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

WebDatabaseService::~WebDatabaseService() {
}

void WebDatabaseService::AddTable(scoped_ptr<WebDatabaseTable> table) {
  if (!wds_backend_) {
    wds_backend_ = new WebDataServiceBackend(path_);
  }
  wds_backend_->AddTable(table.Pass());
}

void WebDatabaseService::LoadDatabase(const InitCallback& callback) {
  DCHECK(wds_backend_);

  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      Bind(&WebDataServiceBackend::InitDatabaseWithCallback,
           wds_backend_, callback));
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
