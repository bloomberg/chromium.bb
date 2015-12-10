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
// under which the registry key for this field trial resides. The |no_pre_read|
// option is set if DLLs should not be pre-read. The |high_priority| option is
// set if pre-reading should be done with a high thread priority. The
// |only_if_cold| option is set if only cold DLLs should be pre-read. The
// |prefetch_virtual_memory| option is set if the
// ::PrefetchVirtualMemory function should be used to pre-read DLLs, if
// available.
void GetPreReadOptions(const base::string16& product_registry_path,
                       bool* no_pre_read,
                       bool* high_priority,
                       bool* only_if_cold,
                       bool* prefetch_virtual_memory);

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
