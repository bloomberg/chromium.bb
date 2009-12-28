// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/sqlite_diagnostics.h"

#include "app/sql/connection.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/singleton.h"

namespace {

const char* kHistogramNames[] = {
  "Sqlite.Cookie.Error",
  "Sqlite.History.Error",
  "Sqlite.Thumbnail.Error",
  "Sqlite.Text.Error",
  "Sqlite.Web.Error"
};

// This class handles the exceptional sqlite errors that we might encounter
// if for example the db is corrupted. Right now we just generate a UMA
// histogram for release and an assert for debug builds.
//
// Why is it a template you ask? well, that is a funny story. The histograms
// need to be singletons that is why they are always static at the function
// scope, but we cannot use the Singleton class because they are not default
// constructible. The template parameter makes the compiler to create unique
// classes that don't share the same static variable.
template <size_t unique>
class BasicSqliteErrrorHandler : public sql::ErrorDelegate {
 public:

  virtual int OnError(int error, sql::Connection* connection,
                      sql::Statement* stmt) {
    NOTREACHED() << "sqlite error " << error;
    RecordErrorInHistogram(error);
    return error;
  }

 private:
  static void RecordErrorInHistogram(int error) {
    // The histogram values from sqlite result codes go currently from 1 to
    // 26 currently but 50 gives them room to grow.
    UMA_HISTOGRAM_ENUMERATION(kHistogramNames[unique], error, 50);
  }
};

}  // namespace

sql::ErrorDelegate* GetErrorHandlerForCookieDb() {
  return new BasicSqliteErrrorHandler<0>();
}

sql::ErrorDelegate* GetErrorHandlerForHistoryDb() {
  return new BasicSqliteErrrorHandler<1>();
}

sql::ErrorDelegate* GetErrorHandlerForThumbnailDb() {
  return new BasicSqliteErrrorHandler<2>();
}

sql::ErrorDelegate* GetErrorHandlerForTextDb() {
  return new BasicSqliteErrrorHandler<3>();
}

sql::ErrorDelegate* GetErrorHandlerForWebDb() {
  return new BasicSqliteErrrorHandler<4>();
}
