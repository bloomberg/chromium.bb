// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/views/first_run_bubble.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class FirstRunBubbleTest : public views::ViewsTestBase {
 public:
  FirstRunBubbleTest();
  ~FirstRunBubbleTest() override;

  // Overrides from views::ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  TestingProfile* profile() { return profile_.get(); }

 private:
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleTest);
};

FirstRunBubbleTest::FirstRunBubbleTest() {}
FirstRunBubbleTest::~FirstRunBubbleTest() {}

void FirstRunBubbleTest::SetUp() {
  ViewsTestBase::SetUp();
  profile_.reset(new TestingProfile());
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(), &TemplateURLServiceFactory::BuildInstanceFor);
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile_.get());
  turl_model->Load();
}

void FirstRunBubbleTest::TearDown() {
  ViewsTestBase::TearDown();
  profile_.reset();
  TestingBrowserProcess::DeleteInstance();
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
