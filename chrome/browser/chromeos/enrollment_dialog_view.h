// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_

#include <string>

#include "ui/gfx/native_widget_types.h"

class Profile;

namespace chromeos {

class EnrollmentDelegate;

EnrollmentDelegate* CreateEnrollmentDelegate(gfx::NativeWindow owning_window,
                                             const std::string& network_name,
                                             Profile* profile);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
