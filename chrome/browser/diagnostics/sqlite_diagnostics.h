// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_

#include "chrome/browser/diagnostics/diagnostics_test.h"

namespace diagnostics {

enum SQLiteIntegrityOutcomeCode {
  DIAG_SQLITE_SUCCESS,
  DIAG_SQLITE_FILE_NOT_FOUND_OK,
  DIAG_SQLITE_FILE_NOT_FOUND,
  DIAG_SQLITE_ERROR_HANDLER_CALLED,
  DIAG_SQLITE_CANNOT_OPEN_DB,
  DIAG_SQLITE_DB_LOCKED,
  DIAG_SQLITE_PRAGMA_FAILED,
  DIAG_SQLITE_DB_CORRUPTED
};

extern const char kSQLiteIntegrityAppCacheTest[];
extern const char kSQLiteIntegrityArchivedHistoryTest[];
extern const char kSQLiteIntegrityCookieTest[];
extern const char kSQLiteIntegrityDatabaseTrackerTest[];
extern const char kSQLiteIntegrityHistoryTest[];
extern const char kSQLiteIntegrityThumbnailsTest[];
extern const char kSQLiteIntegrityWebTest[];

#if defined(OS_CHROMEOS)
extern const char kSQLiteIntegrityNSSCertTest[];
extern const char kSQLiteIntegrityNSSKeyTest[];
#endif

// Factories for the database integrity tests we run in diagnostic mode.
DiagnosticsTest* MakeSqliteWebDbTest();
DiagnosticsTest* MakeSqliteCookiesDbTest();
DiagnosticsTest* MakeSqliteHistoryDbTest();
DiagnosticsTest* MakeSqliteArchivedHistoryDbTest();
DiagnosticsTest* MakeSqliteThumbnailsDbTest();
DiagnosticsTest* MakeSqliteAppCacheDbTest();
DiagnosticsTest* MakeSqliteWebDatabaseTrackerDbTest();

#if defined(OS_CHROMEOS)
DiagnosticsTest* MakeSqliteNssCertDbTest();
DiagnosticsTest* MakeSqliteNssKeyDbTest();
#endif  // defined(OS_CHROMEOS)

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
