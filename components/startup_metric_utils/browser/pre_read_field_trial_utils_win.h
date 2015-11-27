// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_FILE_PRE_READ_FIELD_TRIAL_UTILS_WIN_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_FILE_PRE_READ_FIELD_TRIAL_UTILS_WIN_H_

#include <string>

#include "base/callback_forward.h"
#include "base/strings/string16.h"

// Utility functions to support the PreRead field trial. The PreRead field trial
// changes the way DLLs are pre-read during startup.

namespace startup_metric_utils {

// Callback to register a synthetic field trial.
using RegisterPreReadSyntheticFieldTrialCallback =
    const base::Callback<bool(const std::string&, const std::string&)>;

// Get DLL pre-reading options. |product_registry_path| is the registry path
// under which the registry key for this field trial resides. The
// |should_pre_read| option is set if DLLs should be pre-read before being used.
// The |should_pre_read_high_priority| option is set if this pre-reading should
// be done with a high thread priority.
void GetPreReadOptions(const base::string16& product_registry_path,
                       bool* should_pre_read,
                       bool* should_pre_read_high_priority);

// Updates DLL pre-reading options in the registry with the latest info for the
// next startup. |product_registry_path| is the registry path under which the
// registry key for this field trial resides.
void UpdatePreReadOptions(const base::string16& product_registry_path);

// Registers a synthetic field trial with the PreRead group currently stored in
// the registry. This must be done before the first metric log
// (metrics::MetricsLog) is created. Otherwise, UMA metrics generated during
// startup won't be correctly annotated. |product_registry_path| is the registry
// path under which the key for this field trial resides.
void RegisterPreReadSyntheticFieldTrial(
    const base::string16& product_registry_path,
    const RegisterPreReadSyntheticFieldTrialCallback&
        register_synthetic_field_trial);

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_FILE_PRE_READ_FIELD_TRIAL_UTILS_WIN_H_
