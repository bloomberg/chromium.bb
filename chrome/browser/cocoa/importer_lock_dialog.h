// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_IMPORTER_LOCK_DIALOG_H_
#define CHROME_BROWSER_COCOA_IMPORTER_LOCK_DIALOG_H_

class ImporterHost;

// Bridge from Importer.cc to ImporterLockDialog.
void ImportLockDialogCocoa(ImporterHost* importer);

#endif  // CHROME_BROWSER_COCOA_IMPORTER_LOCK_DIALOG_H_

