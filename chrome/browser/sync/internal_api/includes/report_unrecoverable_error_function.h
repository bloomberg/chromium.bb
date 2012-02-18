// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_INCLUDES_REPORT_UNRECOVERABLE_ERROR_FUNCTION_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_INCLUDES_REPORT_UNRECOVERABLE_ERROR_FUNCTION_H_
#pragma once

namespace browser_sync {

// A ReportUnrecoverableErrorFunction is a function that is called
// immediately when an unrecoverable error is encountered.  Unlike
// UnrecoverableErrorHandler, it should just log the error and any
// context surrounding it.
typedef void (*ReportUnrecoverableErrorFunction)(void);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_INCLUDES_REPORT_UNRECOVERABLE_ERROR_FUNCTION_H_
