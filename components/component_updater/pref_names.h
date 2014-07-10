// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_PREF_NAMES_H_
#define COMPONENTS_COMPONENT_UPDATER_PREF_NAMES_H_

#include "build/build_config.h"

namespace prefs {

extern const char kRecoveryComponentVersion[];

#if defined(OS_WIN)
extern const char kSwReporterExecuteTryCount[];
#endif

}  // namespace prefs

#endif  // COMPONENTS_COMPONENT_UPDATER_PREF_NAMES_H_
