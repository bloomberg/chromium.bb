// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/views_test_base.h"

#if defined(OS_CHROMEOS)
#include "ui/aura/window.h"
#endif

using views::test::InkDropHostViewTestApi;
using views::test::TestInkDrop;

namespace {

const int kStayOpenTimeMS = 100;
const int kOpenTimeMS = 100;
const int kAnimationDurationMS = (kOpenTimeMS * 2) + kStayOpenTimeMS;
const int kImageSize = 15;
const SkColor kTestColor = SkColorSetRGB(64, 64, 64);
const int kNumberOfSteps = 300;

class TestIconLabelBubbleView : public IconLabelBubbleView {
 public:
  enum State {
    GROWING,
    STEADY,
    SHRINKING,
  };

  explicit TestIconLabelBubbleView(const gfx::FontList& font_list)
      : IconLabelBubbleView(font_list), value_(0), is_bubble_showing_(false) {
    GetImageView()->SetImageSize(gfx::Size(kImageSize, kImageSize));
    SetLabel(base::ASCIIToUTF16("Label"));
    separator_view()->set_disable_animation_for_test(true);
  }

  void SetCurrentAnimationValue(int value) {
    value_ = value;
    SizeToPreferredSize();
  }

  int width() const { return bounds().width(); }
  bool IsLabelVisible() const { return label()->visible(); }
  void SetLabelVisible(bool visible) { label()->SetVisible(visible); }
  const gfx::Rect& GetLabelBounds() const { return label()->bounds(); }

  State state() const {
    const double kOpenFraction =
        static_cast<double>(kOpenTimeMS) / kAnimationDurationMS;
    double state = value_ / (double)kNumberOfSteps;
    if (state < kOpenFraction)
      return GROWING;
    if (state > (1.0 - kOpenFraction))
      return SHRINKING;
    return STEADY;
  }

  void HideBubble() {
    OnWidgetVisibilityChanged(nullptr, false);
    is_bubble_showing_ = false;
  }

  bool IsBubbleShowing() const override { return is_bubble_showing_; }

 protected:
  // IconLabelBubbleView:
  SkColor GetTextColor() const override { return kTestColor; }

  bool ShouldShowLabel() const override {
    return !IsShrinking() ||
           (width() > (image()->GetPreferredSize().width() +
                       2 * LocationBarView::kIconInteriorPadding +
                       2 * GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING)));
  }

  double WidthMultiplier() const override {
    const double kOpenFraction =
        static_cast<double>(kOpenTimeMS) / kAnimationDurationMS;
    double fraction = value_ / (double)kNumberOfSteps;
    switch (state()) {
      case GROWING:
        return fraction / kOpenFraction;
      case STEADY:
        return 1.0;
      case SHRINKING:
        return (1.0 - fraction) / kOpenFraction;
    }
    NOTREACHED();
    return 1.0;
  }

  bool IsShrinking() const override { return state() == SHRINKING; }

  bool ShowBubble(const ui::Event& event) override {
    is_bubble_showing_ = true;
    return true;
  }

 private:
  int value_;
  bool is_bubble_showing_;
  DISALLOW_COPY_AND_ASSIGN(TestIconLabelBubbleView);
};

}  // namespace

class IconLabelBubbleViewTest : public views::ViewsTestBase {
 public:
  IconLabelBubbleViewTest()
      : views::ViewsTestBase(),
        widget_(nullptr),
        view_(nullptr),
        ink_drop_(nullptr),
        steady_reached_(false),
        shrinking_reached_(false),
        minimum_size_reached_(false),
        previous_width_(0),
        initial_image_x_(0) {}
  ~IconLabelBubbleViewTest() override {}

 protected:
  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    gfx::FontList font_list;

    CreateWidget();
    generator_.reset(new ui::test::EventGenerator(widget_->GetNativeWindow()));
    view_ = new TestIconLabelBubbleView(font_list);
    view_->SetBoundsRect(gfx::Rect(0, 0, 24, 24));
    widget_->SetContentsView(view_);

    widget_->Show();
  }

  void TearDown() override {
    generator_.reset();
    if (widget_ && !widget_->IsClosed())
      widget_->Close();

    ViewsTestBase::TearDown();
  }

  void VerifyWithAnimationStep(int step) {
    Reset();
    for (int value = 0; value < kNumberOfSteps; value += step) {
      SetValue(value);
      VerifyAnimationStep();
    }
    view_->SetLabelVisible(false);
  }

  TestInkDrop* ink_drop() { return ink_drop_; }

  TestIconLabelBubbleView* view() { return view_; }

  ui::test::EventGenerator* generator() { return generator_.get(); }

  void AttachInkDrop() {
    ink_drop_ = new TestInkDrop();
    InkDropHostViewTestApi(view_).SetInkDrop(base::WrapUnique(ink_drop_));
  }

 private:
  void CreateWidget() {
    DCHECK(!widget_);

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);
  }

  void Reset() {
    view_->SetLabelVisible(true);
    SetValue(0);
    steady_reached_ = false;
    shrinking_reached_ = false;
    minimum_size_reached_ = false;
    previous_width_ = 0;
    initial_image_x_ = GetImageBounds().x();
    EXPECT_EQ(0, initial_image_x_);
  }

  void VerifyAnimationStep() {
    switch (state()) {
      case TestIconLabelBubbleView::State::GROWING: {
        EXPECT_GE(width(), previous_width_);
        EXPECT_EQ(initial_image_x_, GetImageBounds().x());
        EXPECT_GE(GetImageBounds().x(), 0);
        if (GetImageBounds().width() > 0)
          EXPECT_LE(GetImageBounds().right(), width());
        EXPECT_TRUE(IsLabelVisible());
        if (GetLabelBounds().width() > 0) {
          EXPECT_GT(GetLabelBounds().x(), GetImageBounds().right());
          EXPECT_LT(GetLabelBounds().right(), width());
        }
        break;
      }
      case TestIconLabelBubbleView::State::STEADY: {
        if (steady_reached_)
          EXPECT_EQ(previous_width_, width());
        EXPECT_EQ(initial_image_x_, GetImageBounds().x());
        EXPECT_LT(GetImageBounds().right(), width());
        EXPECT_TRUE(IsLabelVisible());
        EXPECT_GT(GetLabelBounds().x(), GetImageBounds().right());
        EXPECT_LT(GetLabelBounds().right(), width());
        steady_reached_ = true;
        break;
      }
      case TestIconLabelBubbleView::State::SHRINKING: {
        if (shrinking_reached_)
          EXPECT_LE(width(), previous_width_);
        if (minimum_size_reached_)
          EXPECT_EQ(previous_width_, width());

        EXPECT_GE(GetImageBounds().x(), 0);
        if (width() <= initial_image_x_ + kImageSize) {
          EXPECT_EQ(width(), GetImageBounds().right());
          EXPECT_EQ(0, GetLabelBounds().width());
        } else {
          EXPECT_EQ(initial_image_x_, GetImageBounds().x());
          EXPECT_LE(GetImageBounds().right(), width());
        }
        if (GetLabelBounds().width() > 0) {
          EXPECT_GT(GetLabelBounds().x(), GetImageBounds().right());
          EXPECT_LT(GetLabelBounds().right(), width());
        }
        shrinking_reached_ = true;
        if (width() == kImageSize)
          minimum_size_reached_ = true;
        break;
      }
    }
    previous_width_ = width();
  }

  void SetValue(int value) { view_->SetCurrentAnimationValue(value); }

  TestIconLabelBubbleView::State state() const { return view_->state(); }

  int width() { return view_->width(); }

  bool IsLabelVisible() { return view_->IsLabelVisible(); }

  const gfx::Rect& GetLabelBounds() const { return view_->GetLabelBounds(); }

  const gfx::Rect& GetImageBounds() const {
    return view_->GetImageView()->bounds();
  }

  views::Widget* widget_;
  TestIconLabelBubbleView* view_;
  TestInkDrop* ink_drop_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

  bool steady_reached_;
  bool shrinking_reached_;
  bool minimum_size_reached_;
  int previous_width_;
  int initial_image_x_;
};

// Tests layout rules for IconLabelBubbleView while simulating animation.
// The animation is first growing the bubble from zero, then keeping its size
// constant and finally shrinking it down to its minimum size which is the image
// size.
// Various step sizes during animation simulate different possible timing.
TEST_F(IconLabelBubbleViewTest, AnimateLayout) {
  VerifyWithAnimationStep(1);
  VerifyWithAnimationStep(5);
  VerifyWithAnimationStep(10);
  VerifyWithAnimationStep(25);
}

// Verify that clicking the view a second time hides its bubble.
TEST_F(IconLabelBubbleViewTest, SecondClick) {
  generator()->PressLeftButton();
  EXPECT_FALSE(view()->IsBubbleShowing());
  generator()->ReleaseLeftButton();
  EXPECT_TRUE(view()->IsBubbleShowing());

  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  generator()->PressLeftButton();
  view()->HideBubble();
  EXPECT_FALSE(view()->IsBubbleShowing());
  generator()->ReleaseLeftButton();
}

TEST_F(IconLabelBubbleViewTest, MouseInkDropState) {
  AttachInkDrop();
  generator()->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            ink_drop()->GetTargetInkDropState());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop()->GetTargetInkDropState());
  view()->HideBubble();
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop()->GetTargetInkDropState());

  // If the bubble is shown, the InkDropState should not change to
  // ACTION_PENDING.
  generator()->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            ink_drop()->GetTargetInkDropState());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop()->GetTargetInkDropState());
  generator()->PressLeftButton();
  EXPECT_NE(views::InkDropState::ACTION_PENDING,
            ink_drop()->GetTargetInkDropState());
}

// Tests the separator opacity. The separator should disappear when there's
// an ink drop. Otherwise, it should be visible.
TEST_F(IconLabelBubbleViewTest, SeparatorOpacity) {
  views::View* separator_view = view()->separator_view();
  separator_view->SetPaintToLayer();
  view()->SetLabel(base::ASCIIToUTF16("x"));
  EXPECT_EQ(1.0f, separator_view->layer()->opacity());

  AttachInkDrop();
  generator()->PressLeftButton();
  view()->InkDropAnimationStarted();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            ink_drop()->GetTargetInkDropState());
  EXPECT_EQ(0.0f, separator_view->layer()->opacity());

  generator()->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop()->GetTargetInkDropState());
  EXPECT_EQ(0.0f, separator_view->layer()->opacity());

  view()->HideBubble();
  view()->InkDropAnimationStarted();
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop()->GetTargetInkDropState());
  EXPECT_EQ(1.0f, separator_view->layer()->opacity());
}

#if !defined(OS_MACOSX)
TEST_F(IconLabelBubbleViewTest, GestureInkDropState) {
  AttachInkDrop();
  generator()->GestureTapAt(gfx::Point());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop()->GetTargetInkDropState());
  view()->HideBubble();
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop()->GetTargetInkDropState());

  // If the bubble is shown, the InkDropState should not change to
  // ACTIVATED.
  generator()->GestureTapAt(gfx::Point());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop()->GetTargetInkDropState());
  generator()->GestureTapAt(gfx::Point());
  view()->HideBubble();
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop()->GetTargetInkDropState());
}
#endif

#if defined(OS_CHROMEOS)
// Verifies IconLabelBubbleView::CalculatePreferredSize() doesn't crash when
// there is a widget but no compositor.
using IconLabelBubbleViewCrashTest = views::ViewsTestBase;

TEST_F(IconLabelBubbleViewCrashTest,
       GetPreferredSizeDoesntCrashWhenNoCompositor) {
  gfx::FontList font_list;
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  views::Widget widget;
  widget.Init(params);
  IconLabelBubbleView* icon_label_bubble_view =
      new TestIconLabelBubbleView(font_list);
  icon_label_bubble_view->SetLabel(base::ASCIIToUTF16("x"));
  widget.GetContentsView()->AddChildView(icon_label_bubble_view);
  aura::Window* widget_native_view = widget.GetNativeView();
  // Remove the window from its parent. This means GetWidget() in
  // IconLabelBubbleView will return non-null, but GetWidget()->GetCompositor()
  // will return null.
  ASSERT_TRUE(widget_native_view->parent());
  widget_native_view->parent()->RemoveChild(widget_native_view);
  static_cast<views::View*>(icon_label_bubble_view)->GetPreferredSize();
}
#endif
