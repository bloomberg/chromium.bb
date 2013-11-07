// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_

#include "base/strings/string16.h"

namespace base {
class Time;
}

namespace installer {

// Constructs a date string in the format understood by Google Update for the
// |current_time| plus one year.
string16 BuildExperimentDateString(const base::Time& current_time);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_
