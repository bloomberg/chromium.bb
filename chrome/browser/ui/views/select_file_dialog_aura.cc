// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/select_file_dialog.h"

#include "base/logging.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener,
                                           ui::SelectFilePolicy* policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(USE_ASH) && defined(FILE_MANAGER_EXTENSION)
  return SelectFileDialogExtension::Create(listener, policy);
#else
  return NULL;
#endif
}
