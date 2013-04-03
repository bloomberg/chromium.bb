// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/sqlite_diagnostics.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/common/content_constants.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/database/database_tracker.h"

namespace {

// Generic diagnostic test class for checking sqlite db integrity.
class SqliteIntegrityTest : public DiagnosticTest {
 public:
  SqliteIntegrityTest(bool critical, const string16& title,
                      const base::FilePath& profile_relative_db_path)
      : DiagnosticTest(title),
        critical_(critical),
        db_path_(profile_relative_db_path) {
  }

  virtual int GetId() OVERRIDE { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) OVERRIDE {
    base::FilePath path = GetUserDefaultProfileDir();
    path = path.Append(db_path_);
    if (!file_util::PathExists(path)) {
      RecordOutcome(ASCIIToUTF16("File not found"),
                    critical_ ? DiagnosticsModel::TEST_FAIL_CONTINUE :
                                DiagnosticsModel::TEST_OK);
      return true;
    }

    int errors = 0;
    { // This block scopes the lifetime of the db objects.
      sql::Connection db;
      db.set_exclusive_locking();
      if (!db.Open(path)) {
        RecordFailure(ASCIIToUTF16("Cannot open DB. Possibly corrupted"));
        return true;
      }
      sql::Statement s(db.GetUniqueStatement("PRAGMA integrity_check;"));
      if (!s.is_valid()) {
        int error = db.GetErrorCode();
        if (SQLITE_BUSY == error) {
          RecordFailure(ASCIIToUTF16("DB locked by another process"));
        } else {
          string16 str(ASCIIToUTF16("Pragma failed. Error: "));
          str += base::IntToString16(error);
          RecordFailure(str);
        }
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
      str += base::IntToString16(errors) + ASCIIToUTF16(" errors");
      RecordFailure(str);
      return true;
    }
    RecordSuccess(ASCIIToUTF16("no corruption detected"));
    return true;
  }

 private:
  bool critical_;
  base::FilePath db_path_;
  DISALLOW_COPY_AND_ASSIGN(SqliteIntegrityTest);
};

}  // namespace

DiagnosticTest* MakeSqliteWebDbTest() {
  return new SqliteIntegrityTest(true, ASCIIToUTF16("Web DB"),
                                 base::FilePath(kWebDataFilename));
}

DiagnosticTest* MakeSqliteCookiesDbTest() {
  return new SqliteIntegrityTest(true, ASCIIToUTF16("Cookies DB"),
                                 base::FilePath(chrome::kCookieFilename));
}

DiagnosticTest* MakeSqliteHistoryDbTest() {
  return new SqliteIntegrityTest(true, ASCIIToUTF16("History DB"),
                                 base::FilePath(chrome::kHistoryFilename));
}

DiagnosticTest* MakeSqliteArchivedHistoryDbTest() {
  return new SqliteIntegrityTest(
      false, ASCIIToUTF16("Archived History DB"),
      base::FilePath(chrome::kArchivedHistoryFilename));
}

DiagnosticTest* MakeSqliteThumbnailsDbTest() {
  return new SqliteIntegrityTest(false, ASCIIToUTF16("Thumbnails DB"),
                                 base::FilePath(chrome::kThumbnailsFilename));
}

DiagnosticTest* MakeSqliteAppCacheDbTest() {
  base::FilePath appcache_dir(content::kAppCacheDirname);
  base::FilePath appcache_db =
      appcache_dir.Append(appcache::kAppCacheDatabaseName);
  return new SqliteIntegrityTest(false, ASCIIToUTF16("AppCache DB"),
                                 appcache_db);
}

DiagnosticTest* MakeSqliteWebDatabaseTrackerDbTest() {
  base::FilePath databases_dir(webkit_database::kDatabaseDirectoryName);
  base::FilePath tracker_db =
      databases_dir.Append(webkit_database::kTrackerDatabaseFileName);
  return new SqliteIntegrityTest(false, ASCIIToUTF16("DatabaseTracker DB"),
                                 tracker_db);
}
