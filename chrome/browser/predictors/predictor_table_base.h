// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTOR_TABLE_BASE_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTOR_TABLE_BASE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/synchronization/cancellation_flag.h"

namespace sql {
class Connection;
}

namespace predictors {

// Base class for all tables in the PredictorDatabase.
//
// Refcounted as it is created and destroyed in the UI thread but all database
// related functions need to happen in the database thread.
class PredictorTableBase
    : public base::RefCountedThreadSafe<PredictorTableBase> {
 protected:
  PredictorTableBase();
  virtual ~PredictorTableBase();

  // DB thread functions.
  virtual void CreateTableIfNonExistent() = 0;
  virtual void LogDatabaseStats() = 0;
  void Initialize(sql::Connection* db);
  sql::Connection* DB();
  void ResetDB();

  bool CantAccessDatabase();

  base::CancellationFlag cancelled_;

 private:
  friend class base::RefCountedThreadSafe<PredictorTableBase>;

  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(PredictorTableBase);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTOR_TABLE_BASE_H_
