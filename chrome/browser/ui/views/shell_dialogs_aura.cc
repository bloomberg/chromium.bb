// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shell_dialogs.h"

#include "chrome/browser/ui/views/file_manager_dialog.h"
#include "content/public/browser/browser_thread.h"

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_WIN)
  return NULL;
#else
  return new FileManagerDialog(listener);
#endif
}
