// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "chrome/browser/predictors/logged_in_predictor_table.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "sql/connection.h"
#include "sql/statement.h"

using content::BrowserThread;

namespace {

// TODO(shishir): This should move to a more generic name.
const base::FilePath::CharType kPredictorDatabaseName[] =
    FILE_PATH_LITERAL("Network Action Predictor");

}  // namespace

namespace predictors {

// Refcounted as it is created, initialized and destroyed on a different thread
// to the DB thread that is required for all methods performing database access.
class PredictorDatabaseInternal
    : public base::RefCountedThreadSafe<PredictorDatabaseInternal> {
 private:
  friend class base::RefCountedThreadSafe<PredictorDatabaseInternal>;
  friend class PredictorDatabase;

  explicit PredictorDatabaseInternal(Profile* profile);
  virtual ~PredictorDatabaseInternal();

  // Opens the database file from the profile path. Separated from the
  // constructor to ease construction/destruction of this object on one thread
  // but database access on the DB thread.
  void Initialize();
  void LogDatabaseStats();  //  DB Thread.

  // Cancels pending DB transactions. Should only be called on the UI thread.
  void SetCancelled();

  bool is_resource_prefetch_predictor_enabled_;
  base::FilePath db_path_;
  scoped_ptr<sql::Connection> db_;

  // TODO(shishir): These tables may not need to be refcounted. Maybe move them
  // to using a WeakPtr instead.
  scoped_refptr<AutocompleteActionPredictorTable> autocomplete_table_;
  scoped_refptr<LoggedInPredictorTable> logged_in_table_;
  scoped_refptr<ResourcePrefetchPredictorTables> resource_prefetch_tables_;

  DISALLOW_COPY_AND_ASSIGN(PredictorDatabaseInternal);
};


PredictorDatabaseInternal::PredictorDatabaseInternal(Profile* profile)
    : db_path_(profile->GetPath().Append(kPredictorDatabaseName)),
      db_(new sql::Connection()),
      autocomplete_table_(new AutocompleteActionPredictorTable()),
      logged_in_table_(new LoggedInPredictorTable()),
      resource_prefetch_tables_(new ResourcePrefetchPredictorTables()) {
  db_->set_histogram_tag("Predictor");
  ResourcePrefetchPredictorConfig config;
  is_resource_prefetch_predictor_enabled_ =
      IsSpeculativeResourcePrefetchingEnabled(profile, &config);
}

PredictorDatabaseInternal::~PredictorDatabaseInternal() {
  // The connection pointer needs to be deleted on the DB thread since there
  // might be a task in progress on the DB thread which uses this connection.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::DB))
    BrowserThread::DeleteSoon(BrowserThread::DB, FROM_HERE, db_.release());
}

void PredictorDatabaseInternal::Initialize() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB) ||
        !BrowserThread::IsMessageLoopValid(BrowserThread::DB));
  // TODO(tburkard): figure out if we need this.
  //  db_->set_exclusive_locking();
  bool success = db_->Open(db_path_);

  if (!success)
    return;

  autocomplete_table_->Initialize(db_.get());
  logged_in_table_->Initialize(db_.get());
  resource_prefetch_tables_->Initialize(db_.get());

  LogDatabaseStats();
}

void PredictorDatabaseInternal::SetCancelled() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
        !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

  autocomplete_table_->SetCancelled();
  logged_in_table_->SetCancelled();
  resource_prefetch_tables_->SetCancelled();
}

void PredictorDatabaseInternal::LogDatabaseStats() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB) ||
        !BrowserThread::IsMessageLoopValid(BrowserThread::DB));

  int64 db_size;
  bool success = base::GetFileSize(db_path_, &db_size);
  DCHECK(success) << "Failed to get file size for " << db_path_.value();
  UMA_HISTOGRAM_MEMORY_KB("PredictorDatabase.DatabaseSizeKB",
                          static_cast<int>(db_size / 1024));

  autocomplete_table_->LogDatabaseStats();
  logged_in_table_->LogDatabaseStats();
  if (is_resource_prefetch_predictor_enabled_)
    resource_prefetch_tables_->LogDatabaseStats();
}

PredictorDatabase::PredictorDatabase(Profile* profile)
    : db_(new PredictorDatabaseInternal(profile)) {
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&PredictorDatabaseInternal::Initialize, db_));
}

PredictorDatabase::~PredictorDatabase() {
}

void PredictorDatabase::Shutdown() {
  db_->SetCancelled();
}

scoped_refptr<AutocompleteActionPredictorTable>
    PredictorDatabase::autocomplete_table() {
  return db_->autocomplete_table_;
}

scoped_refptr<LoggedInPredictorTable>
    PredictorDatabase::logged_in_table() {
  return db_->logged_in_table_;
}

scoped_refptr<ResourcePrefetchPredictorTables>
    PredictorDatabase::resource_prefetch_tables() {
  return db_->resource_prefetch_tables_;
}

sql::Connection* PredictorDatabase::GetDatabase() {
  return db_->db_.get();
}

}  // namespace predictors
