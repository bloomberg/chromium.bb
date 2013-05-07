// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/color_constants.h"
#endif

namespace autofill {

#if defined(TOOLKIT_VIEWS)
TEST(AutofillDialogTypesTest, WarningColorMatches) {
  EXPECT_EQ(kWarningColor, views::kWarningColor);
}
#endif

}  // namespace autofill
