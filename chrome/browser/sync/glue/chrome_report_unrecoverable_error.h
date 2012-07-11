// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHROME_REPORT_UNRECOVERABLE_ERROR_H_
#define CHROME_BROWSER_SYNC_GLUE_CHROME_REPORT_UNRECOVERABLE_ERROR_H_

namespace browser_sync {

// On Windows, sends up a minidump via breakpad.  On other platforms,
// does nothing.
void ChromeReportUnrecoverableError();

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHROME_REPORT_UNRECOVERABLE_ERROR_H_
