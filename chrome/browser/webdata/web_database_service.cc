// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/api/webdata/web_data_results.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_data_request_manager.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_intents_table.h"
// TODO(caitkp): Remove this autofill dependency.
#include "components/autofill/browser/autofill_country.h"

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

  scoped_ptr<AutofillTable> autofill_table_;
  scoped_ptr<KeywordTable> keyword_table_;
  scoped_ptr<LoginsTable> logins_table_;
  scoped_ptr<TokenServiceTable> token_service_table_;
  scoped_ptr<WebAppsTable> web_apps_table_;
  scoped_ptr<WebIntentsTable> web_intents_table_;

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

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceBackend);
};

WebDataServiceBackend::WebDataServiceBackend(const FilePath& path)
    : db_path_(path),
      request_manager_(new WebDataRequestManager()),
      init_status_(sql::INIT_FAILURE),
      init_complete_(false),
      app_locale_(AutofillCountry::ApplicationLocale()) {
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

  // All tables objects that participate in managing the database must
  // be added here.
  autofill_table_.reset(new AutofillTable());
  db_->AddTable(autofill_table_.get());

  keyword_table_.reset(new KeywordTable());
  db_->AddTable(keyword_table_.get());

  // TODO(mdm): We only really need the LoginsTable on Windows for IE7 password
  // access, but for now, we still create it on all platforms since it deletes
  // the old logins table. We can remove this after a while, e.g. in M22 or so.
  logins_table_.reset(new LoginsTable());
  db_->AddTable(logins_table_.get());

  token_service_table_.reset(new TokenServiceTable());
  db_->AddTable(token_service_table_.get());

  web_apps_table_.reset(new WebAppsTable());
  db_->AddTable(web_apps_table_.get());

  // TODO(thakis): Add a migration to delete the SQL table used by
  // WebIntentsTable, then remove this.
  web_intents_table_.reset(new WebIntentsTable());
  db_->AddTable(web_intents_table_.get());

  init_status_ = db_->Init(db_path_, app_locale_);
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
WebDatabaseService::WebDatabaseService(const base::FilePath& path)
    : path_(path) {
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

WebDatabaseService::~WebDatabaseService() {
}

void WebDatabaseService::LoadDatabase(const InitCallback& callback) {
  if (!wds_backend_)
    wds_backend_ = new WebDataServiceBackend(path_);

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

WebDataService::Handle WebDatabaseService::ScheduleDBTaskWithResult(
    const tracked_objects::Location& from_here,
    const ReadTask& task,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  WebDataService::Handle handle = 0;

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
