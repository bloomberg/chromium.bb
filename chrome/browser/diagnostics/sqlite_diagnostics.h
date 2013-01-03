// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_

#include "chrome/browser/diagnostics/diagnostics_test.h"

// Factories for the db integrity tests we run in diagnostic mode.
DiagnosticTest* MakeSqliteWebDbTest();
DiagnosticTest* MakeSqliteCookiesDbTest();
DiagnosticTest* MakeSqliteHistoryDbTest();
DiagnosticTest* MakeSqliteArchivedHistoryDbTest();
DiagnosticTest* MakeSqliteThumbnailsDbTest();
DiagnosticTest* MakeSqliteAppCacheDbTest();
DiagnosticTest* MakeSqliteWebDatabaseTrackerDbTest();

#endif  // CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
