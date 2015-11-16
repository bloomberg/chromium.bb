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
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// Provides functionality to observe the widget passed in the constructor for
// the widget closing event.
class WidgetClosingObserver : public views::WidgetObserver {
 public:
  explicit WidgetClosingObserver(views::Widget* widget)
      : widget_(widget),
        widget_destroyed_(false) {
    widget_->AddObserver(this);
  }

  ~WidgetClosingObserver() override {
    if (widget_)
      widget_->RemoveObserver(this);
  }

  void OnWidgetClosing(views::Widget* widget) override {
    DCHECK(widget == widget_);
    widget_->RemoveObserver(this);
    widget_destroyed_ = true;
    widget_ = nullptr;
  }

  bool widget_destroyed() const {
    return widget_destroyed_;
  }

 private:
  views::Widget* widget_;
  bool widget_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WidgetClosingObserver);
};

class FirstRunBubbleTest : public views::ViewsTestBase,
                           views::WidgetObserver {
 public:
  FirstRunBubbleTest();
  ~FirstRunBubbleTest() override;

  // Overrides from views::ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  void CreateAndCloseBubbleOnEventTest(ui::Event* event);

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

void FirstRunBubbleTest::CreateAndCloseBubbleOnEventTest(ui::Event* event) {
  // Create the anchor and parent widgets.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<views::Widget> anchor_widget(new views::Widget);
  anchor_widget->Init(params);
  anchor_widget->SetBounds(gfx::Rect(10, 10, 500, 500));
  anchor_widget->Show();

  FirstRunBubble* delegate =
      FirstRunBubble::ShowBubble(NULL, anchor_widget->GetContentsView());
  EXPECT_TRUE(delegate != NULL);

  anchor_widget->GetFocusManager()->SetFocusedView(
      anchor_widget->GetContentsView());

  scoped_ptr<WidgetClosingObserver> widget_observer(
      new WidgetClosingObserver(delegate->GetWidget()));

  ui::EventDispatchDetails details =
      anchor_widget->GetNativeWindow()->GetHost()->event_processor()->
          OnEventFromSource(event);
  EXPECT_FALSE(details.dispatcher_destroyed);

  EXPECT_TRUE(widget_observer->widget_destroyed());
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

// Tests that the first run bubble is closed when keyboard events are
// dispatched to the anchor widget.
TEST_F(FirstRunBubbleTest, CloseBubbleOnKeyEvent) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  CreateAndCloseBubbleOnEventTest(&key_event);
}

TEST_F(FirstRunBubbleTest, CloseBubbleOnMouseDownEvent) {
  gfx::Point pt(110, 210);
  ui::MouseEvent mouse_down(
      ui::ET_MOUSE_PRESSED, pt, pt, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  CreateAndCloseBubbleOnEventTest(&mouse_down);
}

TEST_F(FirstRunBubbleTest, CloseBubbleOnTouchDownEvent) {
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 0, ui::EventTimeForNow());
  CreateAndCloseBubbleOnEventTest(&touch_down);
}

