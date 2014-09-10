// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/pref_names.h"

namespace prefs {

// String that represents the recovery component last downloaded version. This
// takes the usual 'a.b.c.d' notation.
const char kRecoveryComponentVersion[] = "recovery_component.version";

#if defined(OS_WIN)
// The last time SwReporter was triggered.
const char kSwReporterLastTimeTriggered[] =
    "software_reporter.last_time_triggered";
#endif

}  // namespace prefs
