// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/importer/importer_lock_dialog.h"

#include "base/message_loop.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/importer/importer.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

void ImportLockDialogCocoa::ShowWarning(ImporterHost* importer) {
  scoped_nsobject<NSAlert> lock_alert([[NSAlert alloc] init]);
  [lock_alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_OK)];
  [lock_alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_CANCEL)];
  [lock_alert setInformativeText:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_TEXT)];
  [lock_alert setMessageText:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_TITLE)];

  if ([lock_alert runModal] == NSAlertFirstButtonReturn) {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        importer, &ImporterHost::OnLockViewEnd, true));
  } else {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        importer, &ImporterHost::OnLockViewEnd, false));
  }
}
