// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_EXPERIMENTS_HELPER_H_
#define CHROME_COMMON_METRICS_EXPERIMENTS_HELPER_H_
#pragma once

namespace ExperimentsHelper {

// Get the current set of chosen FieldTrial groups (aka experiments) and send
// them to the child process logging module so it can save it for crash dumps.
void SetChildProcessLoggingExperimentList();

}  // namespace ExperimentsHelper

#endif  // CHROME_COMMON_METRICS_EXPERIMENTS_HELPER_H_
