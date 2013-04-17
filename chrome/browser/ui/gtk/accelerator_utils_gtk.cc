// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/accelerators_gtk.h"
#include "ui/base/accelerators/accelerator.h"

namespace chrome {

bool IsChromeAccelerator(const ui::Accelerator& accelerator, Profile* profile) {
  AcceleratorsGtk* accelerators = AcceleratorsGtk::GetInstance();
  for (AcceleratorsGtk::const_iterator iter = accelerators->begin();
       iter != accelerators->end(); ++iter) {
    if (iter->second.key_code() == accelerator.key_code() &&
        iter->second.modifiers() == accelerator.modifiers())
      return true;
  }

  return false;
}

}  // namespace chrome
