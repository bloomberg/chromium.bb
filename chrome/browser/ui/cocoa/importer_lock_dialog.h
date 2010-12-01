// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_IMPORTER_LOCK_DIALOG_H_
#define CHROME_BROWSER_UI_COCOA_IMPORTER_LOCK_DIALOG_H_
#pragma once

class ImporterHost;

namespace ImportLockDialogCocoa {

// This function is called by an ImporterHost, and displays the Firefox profile
// locked warning by creating a modal NSAlert.  On the closing of the alert
// box, the ImportHost receives a callback with the message either to skip the
// import, or to try again.
void ShowWarning(ImporterHost* importer);

}

#endif  // CHROME_BROWSER_UI_COCOA_IMPORTER_LOCK_DIALOG_H_
