// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace chromeos {

// An interface that can be used to handle certificate enrollment URIs when
// encountered.  Also used by unit tests to avoid opening browser windows
// when testing.
class EnrollmentDelegate {
 public:
  EnrollmentDelegate() {}
  virtual ~EnrollmentDelegate() {}

  // Implemented to handle a given certificate enrollment URI.  Returns false
  // if the enrollment URI doesn't use a scheme that we can handle.
  // |post_action| is called when enrollment completes.
  virtual bool Enroll(const std::vector<std::string>& uri_list,
                      const base::Closure& post_action) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentDelegate);
};

EnrollmentDelegate* CreateEnrollmentDelegate(gfx::NativeWindow owning_window,
                                             const std::string& network_name,
                                             Profile* profile);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
