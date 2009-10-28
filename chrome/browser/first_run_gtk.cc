// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "chrome/browser/gtk/first_run_dialog.h"

bool OpenFirstRunDialog(Profile* profile, bool homepage_defined,
                        int import_items,
                        int dont_import_items,
                        ProcessSingleton* process_singleton) {
  return FirstRunDialog::Show(profile, process_singleton);
}
