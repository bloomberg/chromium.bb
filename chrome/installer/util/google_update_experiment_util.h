// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_

#include "base/string16.h"

namespace installer {

// Constructs a date string in the format understood by Google Update for the
// current time plus one year.
string16 BuildExperimentDateString();

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_EXPERIMENT_UTIL_H_
