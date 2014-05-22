// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_VARIATIONS_VARIATIONS_UTIL_H_
#define CHROME_COMMON_VARIATIONS_VARIATIONS_UTIL_H_

namespace chrome_variations {

// Get the current set of chosen FieldTrial groups (aka variations) and send
// them to the child process logging module so it can save it for crash dumps.
void SetChildProcessLoggingVariationList();

}  // namespace chrome_variations

#endif  // CHROME_COMMON_VARIATIONS_VARIATIONS_UTIL_H_
