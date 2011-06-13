// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/generic_info_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/widget.h"

// This class is only used on windows for now.
#if defined(OS_WIN)

class GenericInfoViewTest : public testing::Test {
 private:
  MessageLoopForUI message_loop_;
};

TEST_F(GenericInfoViewTest, GenericInfoView) {
  const string16 kName = ASCIIToUTF16("Name");
  const string16 kValue = ASCIIToUTF16("Value");

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(params);
  views::View* root_view = widget->GetRootView();

  GenericInfoView* view1 = new GenericInfoView(1);
  root_view->AddChildView(view1);
  view1->SetName(0, kName);
  view1->SetValue(0, kValue);
  EXPECT_EQ(kName, view1->name_views_[0]->GetText());
  EXPECT_EQ(kValue, view1->value_views_[0]->text());
  view1->ClearValues();
  EXPECT_TRUE(view1->value_views_[0]->text().empty());

  // Test setting values by localized string id.
  static int kNameIds[] = {
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_DESCRIPTION
  };
  GenericInfoView* view2 = new GenericInfoView(ARRAYSIZE(kNameIds), kNameIds);
  root_view->AddChildView(view2);

  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  string16 product_desc = l10n_util::GetStringUTF16(IDS_PRODUCT_DESCRIPTION);
  EXPECT_EQ(product_name, view2->name_views_[0]->GetText());
  EXPECT_EQ(product_desc, view2->name_views_[1]->GetText());
  widget->CloseNow();
}
#endif  // OS_WIN
