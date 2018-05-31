// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_HISTOGRAM_TESTER_H_
#define BASE_TEST_HISTOGRAM_TESTER_H_

// HACK: histogram_tester.h now lives in base/test/metrics. Use transitive
// includes to avoid breaking the compile while we update all other includes.
// TODO(devlin): Remove this once includes are updated as part of
// https://crbug.com/846421
#include "base/test/metrics/histogram_tester.h"

#endif  // BASE_TEST_HISTOGRAM_TESTER_H_
