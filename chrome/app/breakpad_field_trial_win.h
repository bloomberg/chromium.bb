// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_FIELD_TRIAL_WIN_H_
#define CHROME_APP_BREAKPAD_FIELD_TRIAL_WIN_H_
#pragma once

#include <vector>

#include "base/string16.h"

namespace testing {

void SetExperimentList(const std::vector<string16>& experiment_strings);

}  // namespace testing

#endif  // CHROME_APP_BREAKPAD_FIELD_TRIAL_WIN_H_
