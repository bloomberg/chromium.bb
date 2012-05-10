// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "sql/connection.h"
#include "sql/statement.h"

namespace {

// TODO(shishir): This should move to a more generic name.
const FilePath::CharType kPredictorDatabaseName[] =
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

  FilePath db_path_;
  sql::Connection db_;
  scoped_refptr<AutocompleteActionPredictorTable> autocomplete_table_;

  DISALLOW_COPY_AND_ASSIGN(PredictorDatabaseInternal);
};


PredictorDatabaseInternal::PredictorDatabaseInternal(Profile* profile)
    : db_path_(profile->GetPath().Append(kPredictorDatabaseName)),
      autocomplete_table_(new AutocompleteActionPredictorTable()) {
}

PredictorDatabaseInternal::~PredictorDatabaseInternal() {
}

void PredictorDatabaseInternal::Initialize() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  db_.set_exclusive_locking();
  bool success = db_.Open(db_path_);

  if (!success)
    return;

  autocomplete_table_->Initialize(&db_);

  LogDatabaseStats();
}

void PredictorDatabaseInternal::SetCancelled() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  autocomplete_table_->cancelled_.Set();
}

void PredictorDatabaseInternal::LogDatabaseStats() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  int64 db_size;
  bool success = file_util::GetFileSize(db_path_, &db_size);
  DCHECK(success) << "Failed to get file size for " << db_path_.value();
  UMA_HISTOGRAM_MEMORY_KB("PredictorDatabase.DatabaseSizeKB",
                          static_cast<int>(db_size / 1024));

  autocomplete_table_->LogDatabaseStats();
}


PredictorDatabase::PredictorDatabase(Profile* profile)
    : db_(new PredictorDatabaseInternal(profile)) {
  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
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

sql::Connection* PredictorDatabase::GetDatabase() {
  return &db_->db_;
}

}  // namespace predictors
