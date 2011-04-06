// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_PROGRESS_DIALOG_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_PROGRESS_DIALOG_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

class ImporterHost;
class ImporterObserver;
class Profile;

namespace importer {

struct SourceProfile;

// Shows an UI for importing and begins importing the specified |items| from
// |source_profile| to |target_profile|. |importer_observer| is notified when
// the process is complete, it can be NULL. |parent_window| is the window to
// parent the UI to, it can be NULL if there's nothing to parent to. |first_run|
// is true if it's invoked in the first run UI.
void ShowImportProgressDialog(gfx::NativeWindow parent_window,
                              uint16 items,
                              ImporterHost* importer_host,
                              ImporterObserver* importer_observer,
                              const SourceProfile& source_profile,
                              Profile* target_profile,
                              bool first_run);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_PROGRESS_DIALOG_H_
