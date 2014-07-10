// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/pref_names.h"

namespace prefs {

// String that represents the recovery component last downloaded version. This
// takes the usual 'a.b.c.d' notation.
const char kRecoveryComponentVersion[] = "recovery_component.version";

#if defined(OS_WIN)
// The number of attempts left to execute the SwReporter. This starts at the max
// number of retries allowed, and goes down as attempts are made and is cleared
// back to 0 when it successfully completes.
const char kSwReporterExecuteTryCount[] = "software_reporter.execute_try_count";
#endif

}  // namespace prefs
