// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_SWITCHES_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_SWITCHES_H_

#include "build/build_config.h"

namespace spellcheck {
namespace switches {

#if defined(ENABLE_SPELLCHECK)
#if defined(OS_ANDROID)
extern const char kEnableAndroidSpellChecker[];
#endif  // defined(OS_ANDROID)
extern const char kEnableSpellingFeedbackFieldTrial[];
extern const char kSpellingServiceFeedbackIntervalSeconds[];
extern const char kSpellingServiceFeedbackUrl[];
#endif  // defined(ENABLE_SPELLCHECK)

}  // namespace switches
}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_SWITCHES_H_
