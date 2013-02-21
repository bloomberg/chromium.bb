// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

class CommandLine;
class PrefRegistrySyncable;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace print_dialog_cloud {

void RegisterUserPrefs(PrefRegistrySyncable* registry);

// Creates a print dialog to print a file on disk.
// Called on the FILE or UI thread. Even though this may start up a modal
// dialog, it will return immediately. The dialog is handled asynchronously.
// If non-NULL, |modal_parent| specifies a window that the print dialog is modal
// to.
void CreatePrintDialogForFile(content::BrowserContext* browser_context,
                              gfx::NativeWindow modal_parent,
                              const base::FilePath& path_to_file,
                              const string16& print_job_title,
                              const string16& print_ticket,
                              const std::string& file_type,
                              bool delete_on_close);

// Creates a print dialog to print data in RAM.
// Called on the FILE or UI thread. Even though this may start up a modal
// dialog, it will return immediately. The dialog is handled asynchronously.
// If non-NULL, |modal_parent| specifies a window that the print dialog is modal
// to.
void CreatePrintDialogForBytes(content::BrowserContext* browser_context,
                               gfx::NativeWindow modal_parent,
                               const base::RefCountedBytes* data,
                               const string16& print_job_title,
                               const string16& print_ticket,
                               const std::string& file_type);

// Parse switches from command_line and display the print dialog as appropriate.
// Uses the default profile.
bool CreatePrintDialogFromCommandLine(const CommandLine& command_line);

// Creates a dialog for signing into cloud print.
// The dialog will call |callback| when complete.
// Called on the UI thread. Even though this starts up a modal
// dialog, it will return immediately. The dialog is handled asynchronously.
// If non-NULL, |modal_parent| specifies a window that the print dialog is modal
// to.
void CreateCloudPrintSigninDialog(content::BrowserContext* browser_context,
                                  gfx::NativeWindow modal_parent,
                                  const base::Closure& callback);

}  // end namespace

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_H_
