// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SWITCHES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SWITCHES_H_

#include "base/base_switches.h"
#include "base/command_line.h"

namespace switches {

// Switch to enable specific heuristics for detecting if a page is distillable
// or not.
extern const char kReaderModeHeuristics[];

namespace reader_mode_heuristics {
extern const char kAdaBoost[];
extern const char kOGArticle[];
extern const char kAlwaysTrue[];
extern const char kNone[];
};

}  // namespace switches

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SWITCHES_H_
