// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace print_dialog_cloud {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Creates a print dialog to print a file on disk.
// Called on the FILE or UI thread. Even though this may start up a modal
// dialog, it will return immediately. The dialog is handled asynchronously.
// If non-NULL, |modal_parent| specifies a window that the print dialog is modal
// to.
void CreatePrintDialogForFile(content::BrowserContext* browser_context,
                              gfx::NativeWindow modal_parent,
                              const base::FilePath& path_to_file,
                              const base::string16& print_job_title,
                              const base::string16& print_ticket,
                              const std::string& file_type);

// Creates a print dialog to print data in RAM.
// Called on the FILE or UI thread. Even though this may start up a modal
// dialog, it will return immediately. The dialog is handled asynchronously.
// If non-NULL, |modal_parent| specifies a window that the print dialog is modal
// to.
void CreatePrintDialogForBytes(content::BrowserContext* browser_context,
                               gfx::NativeWindow modal_parent,
                               const base::RefCountedMemory* data,
                               const base::string16& print_job_title,
                               const base::string16& print_ticket,
                               const std::string& file_type);

// Parse switches from command_line and display the print dialog as appropriate.
bool CreatePrintDialogFromCommandLine(Profile* profile,
                                      const base::CommandLine& command_line);

// Creates a tab with Google 'sign in' or 'add account' page, based on
// passed |add_account| value.
// Calls |callback| when complete.
void CreateCloudPrintSigninTab(Browser* browser,
                               bool add_account,
                               const base::Closure& callback);

}  // namespace print_dialog_cloud

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_H_
