// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_constants.h"

#include "build/build_config.h"

namespace autofill {

const char kHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_autofill";
#else
    "https://support.google.com/chrome/?p=settings_autofill";
#endif

const size_t kRequiredAutofillFields = 3;

}  // namespace autofill
