// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/views/first_run_bubble.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class FirstRunBubbleTest : public views::ViewsTestBase {
 public:
  FirstRunBubbleTest();
  virtual ~FirstRunBubbleTest();

  // Overrides from views::ViewsTestBase:
  virtual void SetUp() OVERRIDE;

 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
  TemplateURL* default_t_url_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleTest);
};

FirstRunBubbleTest::FirstRunBubbleTest() : default_t_url_(NULL) {}
FirstRunBubbleTest::~FirstRunBubbleTest() {}

void FirstRunBubbleTest::SetUp() {
  ViewsTestBase::SetUp();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &TemplateURLServiceFactory::BuildInstanceFor);
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);
  turl_model->Load();
}

TEST_F(FirstRunBubbleTest, CreateAndClose) {
  // Create the anchor and parent widgets.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<views::Widget> anchor_widget(new views::Widget);
  anchor_widget->Init(params);
  anchor_widget->Show();

  FirstRunBubble* delegate =
      FirstRunBubble::ShowBubble(NULL, anchor_widget->GetContentsView());
  EXPECT_TRUE(delegate != NULL);
  delegate->GetWidget()->CloseNow();
}
