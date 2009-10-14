// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "chrome/browser/gtk/first_run_dialog.h"

bool OpenFirstRunDialog(Profile* profile, bool homepage_defined,
                        ProcessSingleton* process_singleton) {
  // TODO(port): Use process_singleton to make sure Chrome can not be started
  // while this process is active.
  return FirstRunDialog::Show(profile, process_singleton);
}

bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        const FilePath& master_prefs_path,
                                        std::vector<std::wstring>* new_tabs,
                                        int* ping_delay,
                                        bool* homepage_defined) {
  NOTIMPLEMENTED();
  return true;
}

// static
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  // http://code.google.com/p/chromium/issues/detail?id=11971
  return 0;
}
