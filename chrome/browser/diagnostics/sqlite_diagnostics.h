// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
#pragma once

#include "chrome/browser/diagnostics/diagnostics_test.h"

namespace sql {
  class ErrorDelegate;
}

// The following five factories create the error handlers that we use when
// issuing sqlite commands during normal browser operation.
sql::ErrorDelegate* GetErrorHandlerForCookieDb();
sql::ErrorDelegate* GetErrorHandlerForHistoryDb();
sql::ErrorDelegate* GetErrorHandlerForThumbnailDb();
sql::ErrorDelegate* GetErrorHandlerForTextDb();
sql::ErrorDelegate* GetErrorHandlerForWebDb();

// Factories for the db integrity tests we run in diagnostic mode.
DiagnosticTest* MakeSqliteWebDbTest();
DiagnosticTest* MakeSqliteCookiesDbTest();
DiagnosticTest* MakeSqliteHistoryDbTest();
DiagnosticTest* MakeSqliteArchivedHistoryDbTest();
DiagnosticTest* MakeSqliteThumbnailsDbTest();
DiagnosticTest* MakeSqliteAppCacheDbTest();
DiagnosticTest* MakeSqliteWebDatabaseTrackerDbTest();

#endif  // CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
