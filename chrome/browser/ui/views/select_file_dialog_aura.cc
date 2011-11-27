// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/select_file_dialog.h"

#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_WIN)
  return NULL;
#else
  return new SelectFileDialogExtension(listener);
#endif
}
