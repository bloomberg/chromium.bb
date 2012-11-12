// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/infolist_window_view.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/candidate_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

typedef views::ViewsTestBase InfolistWindowViewTest;

TEST_F(InfolistWindowViewTest, ShouldUpdateViewTest) {
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title1");
    new_info->set_description("description1");
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title2");
    new_info->set_description("description1");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title1");
    new_info->set_description("description2");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title2");
    new_info->set_description("description2");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info1 = old_usages.add_information();
    old_info1->set_title("title1");
    old_info1->set_description("description1");
    mozc::commands::Information *old_info2 = old_usages.add_information();
    old_info2->set_title("title2");
    old_info2->set_description("description2");
    mozc::commands::Information *new_info1 = new_usages.add_information();
    new_info1->set_title("title1");
    new_info1->set_description("description1");
    mozc::commands::Information *new_info2 = new_usages.add_information();
    new_info2->set_title("title2");
    new_info2->set_description("description2");
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(0);
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(1);
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info1 = old_usages.add_information();
    old_info1->set_title("title1");
    old_info1->set_description("description1");
    mozc::commands::Information *old_info2 = old_usages.add_information();
    old_info2->set_title("title2");
    old_info2->set_description("description2");
    mozc::commands::Information *new_info1 = new_usages.add_information();
    new_info1->set_title("title1");
    new_info1->set_description("description1");
    mozc::commands::Information *new_info2 = new_usages.add_information();
    new_info2->set_title("title3");
    new_info2->set_description("description3");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(0);
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(1);
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
}

}  // namespace input_method
}  // namespace chromeos
