// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocheckout_page_meta_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetPageDetails(autofill::AutocheckoutPageMetaData* meta_data,
                    int current_page,
                    int total) {
  meta_data->current_page_number = current_page;
  meta_data->total_pages = total;
}

}  // namespace

namespace autofill {

TEST(AutocheckoutPageMetaDataTest, AutofillableFlow) {

  AutocheckoutPageMetaData page_meta_data;
  EXPECT_FALSE(page_meta_data.IsStartOfAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsInAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsEndOfAutofillableFlow());

  SetPageDetails(&page_meta_data, -1, 0);
  EXPECT_FALSE(page_meta_data.IsStartOfAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsInAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsEndOfAutofillableFlow());

  SetPageDetails(&page_meta_data, 0, 0);
  EXPECT_FALSE(page_meta_data.IsStartOfAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsInAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsEndOfAutofillableFlow());

  SetPageDetails(&page_meta_data, 0, 1);
  EXPECT_TRUE(page_meta_data.IsStartOfAutofillableFlow());
  EXPECT_TRUE(page_meta_data.IsInAutofillableFlow());
  EXPECT_TRUE(page_meta_data.IsEndOfAutofillableFlow());

  SetPageDetails(&page_meta_data, 1, 2);
  EXPECT_FALSE(page_meta_data.IsStartOfAutofillableFlow());
  EXPECT_TRUE(page_meta_data.IsInAutofillableFlow());
  EXPECT_TRUE(page_meta_data.IsEndOfAutofillableFlow());

  SetPageDetails(&page_meta_data, 2, 2);
  EXPECT_FALSE(page_meta_data.IsStartOfAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsInAutofillableFlow());
  EXPECT_FALSE(page_meta_data.IsEndOfAutofillableFlow());
}

}  // namespace autofill
