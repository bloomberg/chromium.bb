// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_table_base.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "sql/connection.h"

using content::BrowserThread;

namespace predictors {

PredictorTableBase::PredictorTableBase() : db_(NULL) {
}

PredictorTableBase::~PredictorTableBase() {
}

void PredictorTableBase::Initialize(sql::Connection* db) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_ = db;
  CreateTableIfNonExistent();
}

void PredictorTableBase::SetCancelled() {
  cancelled_.Set();
}

sql::Connection* PredictorTableBase::DB() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return db_;
}

void PredictorTableBase::ResetDB() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_ = NULL;
}

bool PredictorTableBase::CantAccessDatabase() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return cancelled_.IsSet() || !db_;
}

}  // namespace predictors
