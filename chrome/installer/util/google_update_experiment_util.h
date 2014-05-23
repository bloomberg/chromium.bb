// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_

#include "base/strings/string16.h"

namespace base {
class Time;
}

namespace google_update {

#if defined(OS_WIN)
// The name of the value where Google Update reads the list of experiments for
// itself and Chrome.
extern const wchar_t kExperimentLabels[];
#endif

// The separator used to separate items in kExperimentLabels.
extern const base::char16 kExperimentLabelSeparator;

}  // namespace google_update

namespace installer {

// Constructs a date string in the format understood by Google Update for the
// |current_time| plus one year.
base::string16 BuildExperimentDateString(const base::Time& current_time);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_
