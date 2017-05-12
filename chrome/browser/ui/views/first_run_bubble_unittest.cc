// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_bubble.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class FirstRunBubbleTest : public views::ViewsTestBase {
 public:
  FirstRunBubbleTest();
  ~FirstRunBubbleTest() override;

  // Overrides from views::ViewsTestBase:
  void SetUp() override;

  void CreateAndCloseBubbleOnEventTest(ui::Event* event);

 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  // TestBrowserThreadBundle cannot be used here because the ViewsTestBase
  // superclass creates its own MessageLoop.
  content::TestBrowserThread ui_thread_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleTest);
};

FirstRunBubbleTest::FirstRunBubbleTest()
    : ui_thread_(content::BrowserThread::UI, base::MessageLoop::current()) {}

FirstRunBubbleTest::~FirstRunBubbleTest() {}

void FirstRunBubbleTest::SetUp() {
  ViewsTestBase::SetUp();
  // Set the ChromeLayoutProvider as the default layout provider.
  test_views_delegate()->set_layout_provider(
      ChromeLayoutProvider::CreateLayoutProvider());
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &TemplateURLServiceFactory::BuildInstanceFor);
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile());
  turl_model->Load();
}

void FirstRunBubbleTest::CreateAndCloseBubbleOnEventTest(ui::Event* event) {
  // Create the anchor and parent widgets.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  views::Widget anchor_widget;
  anchor_widget.Init(params);
  anchor_widget.SetBounds(gfx::Rect(10, 10, 500, 500));
  anchor_widget.Show();

  FirstRunBubble* delegate =
      FirstRunBubble::ShowBubble(nullptr, anchor_widget.GetContentsView());
  EXPECT_TRUE(delegate);

  anchor_widget.GetContentsView()->RequestFocus();

  views::test::WidgetClosingObserver widget_observer(delegate->GetWidget());

  ui::EventDispatchDetails details = anchor_widget.GetNativeWindow()
                                         ->GetHost()
                                         ->event_sink()
                                         ->OnEventFromSource(event);
  EXPECT_FALSE(details.dispatcher_destroyed);

  EXPECT_TRUE(widget_observer.widget_closed());
}

TEST_F(FirstRunBubbleTest, CreateAndClose) {
  // Create the anchor and parent widgets.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  views::Widget anchor_widget;
  anchor_widget.Init(params);
  anchor_widget.Show();

  FirstRunBubble* delegate =
      FirstRunBubble::ShowBubble(nullptr, anchor_widget.GetContentsView());
  EXPECT_TRUE(delegate);
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
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  CreateAndCloseBubbleOnEventTest(&touch_down);
}

