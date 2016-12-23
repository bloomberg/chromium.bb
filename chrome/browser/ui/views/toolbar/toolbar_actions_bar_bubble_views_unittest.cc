// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
const int kIconSize = 16;
}

class ToolbarActionsBarBubbleViewsTest : public views::ViewsTestBase {
 public:
  views::View* TestCreateExtraView() {
    DCHECK(bubble_);
    return bubble_->CreateExtraView();
  }

 protected:
  ToolbarActionsBarBubbleViewsTest() {}
  ~ToolbarActionsBarBubbleViewsTest() override {}

  void TearDown() override {
    anchor_widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> CreateAnchorWidget() {
    std::unique_ptr<views::Widget> anchor_widget(new views::Widget());
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget->Init(params);
    return anchor_widget;
  }

  void ShowBubble(TestToolbarActionsBarBubbleDelegate* delegate) {
    ASSERT_TRUE(delegate);
    ASSERT_FALSE(bubble_widget_);
    ASSERT_FALSE(bubble_);
    anchor_widget_ = CreateAnchorWidget();
    bubble_ = new ToolbarActionsBarBubbleViews(
      anchor_widget_->GetContentsView(), delegate->GetDelegate());
    bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_);
    bubble_->Show();
  }

  void CloseBubble() {
    ASSERT_TRUE(bubble_);
    bubble_->GetWidget()->Close();
    base::RunLoop().RunUntilIdle();
    bubble_ = nullptr;
    bubble_widget_ = nullptr;
  }

  void ClickView(const views::View* view) {
    ASSERT_TRUE(view);
    ui::test::EventGenerator generator(GetContext(),
                                       anchor_widget_->GetNativeWindow());
    generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
    base::RunLoop().RunUntilIdle();
  }

  base::string16 HeadingString() { return base::ASCIIToUTF16("Heading"); }
  base::string16 BodyString() { return base::ASCIIToUTF16("Body"); }
  base::string16 ActionString() { return base::ASCIIToUTF16("Action"); }
  base::string16 DismissString() { return base::ASCIIToUTF16("Dismiss"); }
  base::string16 LearnMoreString() { return base::ASCIIToUTF16("Learn"); }
  base::string16 ItemListString() {
    return base::ASCIIToUTF16("Item 1\nItem2");
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }
  views::Widget* bubble_widget() { return bubble_widget_; }
  ToolbarActionsBarBubbleViews* bubble() { return bubble_; }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
  views::Widget* bubble_widget_ = nullptr;
  ToolbarActionsBarBubbleViews* bubble_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarBubbleViewsTest);
};

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutActionButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_FALSE(bubble()->GetDialogClientView()->cancel_button());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutNoButtons) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info =
          base::MakeUnique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  delegate.set_extra_view_info(std::move(extra_view_info));
  delegate.set_dismiss_button_text(base::string16());
  delegate.set_action_button_text(base::string16());
  ShowBubble(&delegate);

  std::unique_ptr<views::View> extra_view(TestCreateExtraView());
  EXPECT_FALSE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_FALSE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_FALSE(bubble()->learn_more_button());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionAndDismissButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_TRUE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_EQ(DismissString(),
            bubble()->GetDialogClientView()->cancel_button()->GetText());

  EXPECT_FALSE(bubble()->learn_more_button());
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionDismissAndLearnMoreButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info_linked_text =
          base::MakeUnique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info_linked_text->text = LearnMoreString();
  extra_view_info_linked_text->is_text_linked = true;
  delegate.set_extra_view_info(std::move(extra_view_info_linked_text));

  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_TRUE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_EQ(DismissString(),
            bubble()->GetDialogClientView()->cancel_button()->GetText());
  EXPECT_TRUE(bubble()->learn_more_button());
  EXPECT_EQ(LearnMoreString(), bubble()->learn_more_button()->text());
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutListView) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_item_list_text(ItemListString());
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_FALSE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_FALSE(bubble()->learn_more_button());
  EXPECT_TRUE(bubble()->item_list());
  EXPECT_EQ(ItemListString(), bubble()->item_list()->text());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestShowAndCloseBubble) {
  std::unique_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());

  EXPECT_FALSE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble);
  views::test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble->Show();
  EXPECT_TRUE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());

  bubble->GetDialogClientView()->CancelWindow();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickActionButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  ClickView(bubble()->GetDialogClientView()->ok_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickDismissButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  ClickView(bubble()->GetDialogClientView()->cancel_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickLearnMoreLink) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  ClickView(bubble()->learn_more_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE,
            *delegate.close_action());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCloseOnDeactivation) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  // Close the bubble by activating another widget. The delegate should be
  // told it was dismissed.
  anchor_widget()->Activate();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestDontCloseOnDeactivation) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_close_on_deactivate(false);
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  // Activate another widget. The bubble shouldn't close.
  anchor_widget()->Activate();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate.close_action());
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestNullExtraView) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  ShowBubble(&delegate);
  std::unique_ptr<views::View> extra_view(TestCreateExtraView());
  ASSERT_FALSE(extra_view);
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewIconOnly) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info =
          base::MakeUnique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->resource_id = gfx::VectorIconId::BUSINESS;
  delegate.set_extra_view_info(std::move(extra_view_info));
  ShowBubble(&delegate);
  std::unique_ptr<views::View> extra_view(TestCreateExtraView());
  ASSERT_TRUE(extra_view);
  ASSERT_EQ("ImageView", std::string(extra_view->GetClassName()));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(static_cast<views::ImageView*>(extra_view.get())->GetImage()),
      gfx::Image(gfx::CreateVectorIcon(gfx::VectorIconId::BUSINESS, kIconSize,
                                       gfx::kChromeIconGrey))));
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewLinkedTextOnly) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info_linked_text =
          base::MakeUnique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info_linked_text->text =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN);
  extra_view_info_linked_text->is_text_linked = true;
  delegate.set_extra_view_info(std::move(extra_view_info_linked_text));

  ShowBubble(&delegate);

  std::unique_ptr<views::View> extra_view(TestCreateExtraView());
  ASSERT_TRUE(extra_view);
  ASSERT_EQ("Link", std::string(extra_view->GetClassName()));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN),
            static_cast<views::Label*>(extra_view.get())->text());
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewLabelTextOnly) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info =
          base::MakeUnique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->text =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN);
  extra_view_info->is_text_linked = false;
  delegate.set_extra_view_info(std::move(extra_view_info));

  ShowBubble(&delegate);

  std::unique_ptr<views::View> extra_view(TestCreateExtraView());
  ASSERT_TRUE(extra_view);
  EXPECT_EQ("Label", std::string(extra_view->GetClassName()));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN),
            static_cast<views::Label*>(extra_view.get())->text());
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewImageAndText) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info =
          base::MakeUnique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->resource_id = gfx::VectorIconId::BUSINESS;
  extra_view_info->text =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN);
  extra_view_info->is_text_linked = false;
  delegate.set_extra_view_info(std::move(extra_view_info));

  ShowBubble(&delegate);

  std::unique_ptr<views::View> extra_view(TestCreateExtraView());
  ASSERT_TRUE(extra_view);
  EXPECT_EQ("View", std::string(extra_view->GetClassName()));
  EXPECT_EQ(2, extra_view->child_count());

  for (int i = 0; i < 2; i++) {
    const views::View* v = extra_view->child_at(i);
    std::string class_name = v->GetClassName();
    ASSERT_TRUE(class_name == "Label" || class_name == "ImageView");
    if (class_name == "Label") {
      EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN),
                static_cast<const views::Label*>(v)->text());
    } else {
      EXPECT_TRUE(gfx::test::AreImagesEqual(
          gfx::Image(static_cast<const views::ImageView*>(v)->GetImage()),
          gfx::Image(gfx::CreateVectorIcon(gfx::VectorIconId::BUSINESS,
                                           kIconSize, gfx::kChromeIconGrey))));
    }
  }

  CloseBubble();
}
