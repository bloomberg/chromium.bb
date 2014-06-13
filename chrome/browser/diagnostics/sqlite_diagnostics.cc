// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/sqlite_diagnostics.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_constants.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/common/content_constants.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"
#include "webkit/browser/database/database_tracker.h"

namespace diagnostics {

namespace {

// Generic diagnostic test class for checking SQLite database integrity.
class SqliteIntegrityTest : public DiagnosticsTest {
 public:
  // These are bit flags, so each value should be a power of two.
  enum Flags {
    NO_FLAGS_SET = 0,
    CRITICAL = 0x01,
    REMOVE_IF_CORRUPT = 0x02,
  };

  SqliteIntegrityTest(uint32 flags,
                      DiagnosticsTestId id,
                      const base::FilePath& db_path)
      : DiagnosticsTest(id), flags_(flags), db_path_(db_path) {}

  virtual bool RecoveryImpl(DiagnosticsModel::Observer* observer) OVERRIDE {
    int outcome_code = GetOutcomeCode();
    if (flags_ & REMOVE_IF_CORRUPT) {
      switch (outcome_code) {
        case DIAG_SQLITE_ERROR_HANDLER_CALLED:
        case DIAG_SQLITE_CANNOT_OPEN_DB:
        case DIAG_SQLITE_DB_LOCKED:
        case DIAG_SQLITE_PRAGMA_FAILED:
        case DIAG_SQLITE_DB_CORRUPTED:
          LOG(WARNING) << "Removing broken SQLite database: "
                       << db_path_.value();
          base::DeleteFile(db_path_, false);
          break;
        case DIAG_SQLITE_SUCCESS:
        case DIAG_SQLITE_FILE_NOT_FOUND_OK:
        case DIAG_SQLITE_FILE_NOT_FOUND:
          break;
        default:
          DCHECK(false) << "Invalid outcome code: " << outcome_code;
          break;
      }
    }
    return true;
  }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) OVERRIDE {
    // If we're given an absolute path, use it. If not, then assume it's under
    // the profile directory.
    base::FilePath path;
    if (!db_path_.IsAbsolute())
      path = GetUserDefaultProfileDir().Append(db_path_);
    else
      path = db_path_;

    if (!base::PathExists(path)) {
      if (flags_ & CRITICAL) {
        RecordOutcome(DIAG_SQLITE_FILE_NOT_FOUND,
                      "File not found",
                      DiagnosticsModel::TEST_FAIL_CONTINUE);
      } else {
        RecordOutcome(DIAG_SQLITE_FILE_NOT_FOUND_OK,
                      "File not found (but that is OK)",
                      DiagnosticsModel::TEST_OK);
      }
      return true;
    }

    int errors = 0;
    {  // Scope the statement and database so they close properly.
      sql::Connection database;
      database.set_exclusive_locking();
      scoped_refptr<ErrorRecorder> recorder(new ErrorRecorder);

      // Set the error callback so that we can get useful results in a debug
      // build for a corrupted database. Without setting the error callback,
      // sql::Connection will just DCHECK.
      database.set_error_callback(
          base::Bind(&SqliteIntegrityTest::ErrorRecorder::RecordSqliteError,
                     recorder->AsWeakPtr(),
                     &database));
      if (!database.Open(path)) {
        RecordFailure(DIAG_SQLITE_CANNOT_OPEN_DB,
                      "Cannot open DB. Possibly corrupted");
        return true;
      }
      if (recorder->has_error()) {
        RecordFailure(DIAG_SQLITE_ERROR_HANDLER_CALLED,
                      recorder->FormatError());
        return true;
      }
      sql::Statement statement(
          database.GetUniqueStatement("PRAGMA integrity_check;"));
      if (recorder->has_error()) {
        RecordFailure(DIAG_SQLITE_ERROR_HANDLER_CALLED,
                      recorder->FormatError());
        return true;
      }
      if (!statement.is_valid()) {
        int error = database.GetErrorCode();
        if (SQLITE_BUSY == error) {
          RecordFailure(DIAG_SQLITE_DB_LOCKED,
                        "Database locked by another process");
        } else {
          std::string str("Pragma failed. Error: ");
          str += base::IntToString(error);
          RecordFailure(DIAG_SQLITE_PRAGMA_FAILED, str);
        }
        return false;
      }

      while (statement.Step()) {
        std::string result(statement.ColumnString(0));
        if ("ok" != result)
          ++errors;
      }
      if (recorder->has_error()) {
        RecordFailure(DIAG_SQLITE_ERROR_HANDLER_CALLED,
                      recorder->FormatError());
        return true;
      }
    }

    // All done. Report to the user.
    if (errors != 0) {
      std::string str("Database corruption detected: ");
      str += base::IntToString(errors) + " errors";
      RecordFailure(DIAG_SQLITE_DB_CORRUPTED, str);
      return true;
    }
    RecordSuccess("No corruption detected");
    return true;
  }

 private:
  class ErrorRecorder : public base::RefCounted<ErrorRecorder>,
                        public base::SupportsWeakPtr<ErrorRecorder> {
   public:
    ErrorRecorder() : has_error_(false), sqlite_error_(0), last_errno_(0) {}

    void RecordSqliteError(sql::Connection* connection,
                           int sqlite_error,
                           sql::Statement* statement) {
      has_error_ = true;
      sqlite_error_ = sqlite_error;
      last_errno_ = connection->GetLastErrno();
      message_ = connection->GetErrorMessage();
    }

    bool has_error() const { return has_error_; }

    std::string FormatError() {
      return base::StringPrintf("SQLite error: %d, Last Errno: %d: %s",
                                sqlite_error_,
                                last_errno_,
                                message_.c_str());
    }

   private:
    friend class base::RefCounted<ErrorRecorder>;
    ~ErrorRecorder() {}

    bool has_error_;
    int sqlite_error_;
    int last_errno_;
    std::string message_;

    DISALLOW_COPY_AND_ASSIGN(ErrorRecorder);
  };

  uint32 flags_;
  base::FilePath db_path_;
  DISALLOW_COPY_AND_ASSIGN(SqliteIntegrityTest);
};

}  // namespace

DiagnosticsTest* MakeSqliteCookiesDbTest() {
  return new SqliteIntegrityTest(SqliteIntegrityTest::CRITICAL,
                                 DIAGNOSTICS_SQLITE_INTEGRITY_COOKIE_TEST,
                                 base::FilePath(chrome::kCookieFilename));
}

DiagnosticsTest* MakeSqliteWebDatabaseTrackerDbTest() {
  base::FilePath databases_dir(webkit_database::kDatabaseDirectoryName);
  base::FilePath tracker_db =
      databases_dir.Append(webkit_database::kTrackerDatabaseFileName);
  return new SqliteIntegrityTest(
      SqliteIntegrityTest::NO_FLAGS_SET,
      DIAGNOSTICS_SQLITE_INTEGRITY_DATABASE_TRACKER_TEST,
      tracker_db);
}

DiagnosticsTest* MakeSqliteHistoryDbTest() {
  return new SqliteIntegrityTest(SqliteIntegrityTest::CRITICAL,
                                 DIAGNOSTICS_SQLITE_INTEGRITY_HISTORY_TEST,
                                 base::FilePath(chrome::kHistoryFilename));
}

#if defined(OS_CHROMEOS)
DiagnosticsTest* MakeSqliteNssCertDbTest() {
  base::FilePath home_dir;
  PathService::Get(base::DIR_HOME, &home_dir);
  return new SqliteIntegrityTest(SqliteIntegrityTest::REMOVE_IF_CORRUPT,
                                 DIAGNOSTICS_SQLITE_INTEGRITY_NSS_CERT_TEST,
                                 home_dir.Append(chromeos::kNssCertDbPath));
}

DiagnosticsTest* MakeSqliteNssKeyDbTest() {
  base::FilePath home_dir;
  PathService::Get(base::DIR_HOME, &home_dir);
  return new SqliteIntegrityTest(SqliteIntegrityTest::REMOVE_IF_CORRUPT,
                                 DIAGNOSTICS_SQLITE_INTEGRITY_NSS_KEY_TEST,
                                 home_dir.Append(chromeos::kNssKeyDbPath));
}
#endif  // defined(OS_CHROMEOS)

DiagnosticsTest* MakeSqliteThumbnailsDbTest() {
  return new SqliteIntegrityTest(SqliteIntegrityTest::NO_FLAGS_SET,
                                 DIAGNOSTICS_SQLITE_INTEGRITY_THUMBNAILS_TEST,
                                 base::FilePath(chrome::kThumbnailsFilename));
}

DiagnosticsTest* MakeSqliteWebDataDbTest() {
  return new SqliteIntegrityTest(SqliteIntegrityTest::CRITICAL,
                                 DIAGNOSTICS_SQLITE_INTEGRITY_WEB_DATA_TEST,
                                 base::FilePath(kWebDataFilename));
}

}  // namespace diagnostics
