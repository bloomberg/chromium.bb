// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_TEST_UTIL_H_
#pragma once

#include <ostream>

#include "chrome/browser/sync/syncable/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncable {

// Defined for googletest.  Forwards to ModelEnumSetToString().
void PrintTo(ModelEnumSet model_types, ::std::ostream* os);

// A gmock matcher for ModelEnumSet.  Use like:
//
//   EXPECT_CALL(mock, ProcessModelTypes(HasModelTypes(expected_types)));
::testing::Matcher<ModelEnumSet> HasModelTypes(ModelEnumSet expected_types);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_TEST_UTIL_H_
