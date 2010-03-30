// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/sqlite_diagnostics.h"

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/statement.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

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

struct DbTestInfo {
  const char* test_name;
  const FilePath::CharType* db_name;
};

static const DbTestInfo kTestInfo[] = {
  {"Web Database", chrome::kWebDataFilename},
  {"Cookies Database", chrome::kCookieFilename},
  {"History Database", chrome::kHistoryFilename},
  {"Archived history Database", chrome::kArchivedHistoryFilename},
  {"Thumbnails Database", chrome::kThumbnailsFilename}
};

// Generic diagnostic test class for checking sqlite db integrity.
class SqliteIntegrityTest : public DiagnosticTest {
 public:
  explicit SqliteIntegrityTest(int index)
      : DiagnosticTest(ASCIIToUTF16(kTestInfo[index].test_name)),
        index_(index) {
  }

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    FilePath path;
    PathService::Get(chrome::DIR_USER_DATA, &path);
    path = path.Append(FilePath::FromWStringHack(chrome::kNotSignedInProfile));
    path = path.Append(kTestInfo[index_].db_name);
    if (!file_util::PathExists(path)) {
      RecordFailure(ASCIIToUTF16("File not found"));
      return true;
    }

    int errors = 0;
    { // This block scopes the lifetime of the db objects.
      sql::Connection db;
      db.set_exclusive_locking();
      if (!db.Open(path)) {
        RecordFailure(ASCIIToUTF16("Cannot open db. Possibly corrupted"));
        return true;
      }
      sql::Statement s(db.GetUniqueStatement("PRAGMA integrity_check;"));
      if (!s) {
        RecordFailure(ASCIIToUTF16("Statement failed"));
        return false;
      }
      while (s.Step()) {
        std::string result(s.ColumnString(0));
        if ("ok" != result)
          ++errors;
      }
    }
    // All done. Report to the user.
    if (errors != 0) {
      string16 str(ASCIIToUTF16("Database corruption detected :"));
      str += IntToString16(errors) + ASCIIToUTF16(" errors");
      RecordFailure(str);
      return true;
    }
    RecordSuccess(ASCIIToUTF16("no corruption detected"));
    return true;
  }

 private:
  int index_;
  DISALLOW_COPY_AND_ASSIGN(SqliteIntegrityTest);
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

DiagnosticTest* MakeSqliteWebDbTest() {
  return new SqliteIntegrityTest(0);
}

DiagnosticTest* MakeSqliteCookiesDbTest() {
  return new SqliteIntegrityTest(1);
}

DiagnosticTest* MakeSqliteHistoryDbTest() {
  return new SqliteIntegrityTest(2);
}

DiagnosticTest* MakeSqliteArchivedHistoryDbTest() {
  return new SqliteIntegrityTest(3);
}

DiagnosticTest* MakeSqliteThumbnailsDbTest() {
  return new SqliteIntegrityTest(4);
}

