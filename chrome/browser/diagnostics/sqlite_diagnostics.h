// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_

namespace sql {
  class ErrorDelegate;
}

sql::ErrorDelegate* GetErrorHandlerForCookieDb();

sql::ErrorDelegate* GetErrorHandlerForHistoryDb();

sql::ErrorDelegate* GetErrorHandlerForThumbnailDb();

sql::ErrorDelegate* GetErrorHandlerForTextDb();

sql::ErrorDelegate* GetErrorHandlerForWebDb();

#endif  // CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
