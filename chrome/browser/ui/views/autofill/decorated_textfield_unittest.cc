// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"

namespace autofill {

TEST(DecoratedTextfieldTest, HeightMatchesButton) {
  DecoratedTextfield textfield(ASCIIToUTF16("default"),
                               ASCIIToUTF16("placeholder"),
                               NULL);
  views::LabelButton button(NULL, ASCIIToUTF16("anyoldtext"));;
  button.SetStyle(views::Button::STYLE_BUTTON);
  EXPECT_EQ(button.GetPreferredSize().height() -
                DecoratedTextfield::kMagicInsetNumber,
            textfield.GetPreferredSize().height());
}

}  // namespace autofill
