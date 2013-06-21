// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_WIN_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/component_updater/component_patcher.h"

class ComponentPatcherWin : public ComponentPatcher {
 public:
  ComponentPatcherWin() {}
  virtual ComponentUnpacker::Error Patch(PatchType patch_type,
                                         const base::FilePath& input_file,
                                         const base::FilePath& patch_file,
                                         const base::FilePath& output_file,
                                         int* error) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentPatcherWin);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_WIN_H_
