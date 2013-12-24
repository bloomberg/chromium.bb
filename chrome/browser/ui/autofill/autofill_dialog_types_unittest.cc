// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
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

// Tests for correct parsing of anchor text ranges.
TEST(AutofillDialogTypesTest, DialogNotificationLink) {
  base::string16 text(base::ASCIIToUTF16("Notification without anchor text"));
  DialogNotification notification(DialogNotification::WALLET_ERROR, text);
  EXPECT_TRUE(notification.link_range().is_empty());

  text = base::ASCIIToUTF16("Notification with |anchor text|");
  notification = DialogNotification(DialogNotification::WALLET_ERROR, text);
  base::char16 bar = '|';
  EXPECT_EQ(base::string16::npos, notification.display_text().find(bar));
  EXPECT_FALSE(notification.link_range().is_empty());
}

}  // namespace autofill
