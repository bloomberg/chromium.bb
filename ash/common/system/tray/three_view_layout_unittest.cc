// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/three_view_layout.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/test/test_layout_manager.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace ash {

class ThreeViewLayoutTest : public testing::Test {
 public:
  ThreeViewLayoutTest();

 protected:
  // Returns the bounds of |child| in the coordinate space of |host_|.
  gfx::Rect GetBoundsInHost(const views::View* child) const;

  // Wrapper functions to access the internals of |layout_|.
  views::View* GetContainer(ThreeViewLayout::Container container) const;

  // The test target.
  ThreeViewLayout* layout_;

  // The View that the test target is installed on.
  std::unique_ptr<views::View> host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreeViewLayoutTest);
};

ThreeViewLayoutTest::ThreeViewLayoutTest()
    : layout_(new ThreeViewLayout()), host_(new views::View) {
  host_->SetLayoutManager(layout_);
}

gfx::Rect ThreeViewLayoutTest::GetBoundsInHost(const views::View* child) const {
  gfx::RectF rect_f(child->bounds());
  views::View::ConvertRectToTarget(child, host_.get(), &rect_f);
  return ToNearestRect(rect_f);
}

views::View* ThreeViewLayoutTest::GetContainer(
    ThreeViewLayout::Container container) const {
  return layout_->GetContainer(container);
}

// Confirms there are no crashes when destroying the host view before the layout
// manager.
TEST_F(ThreeViewLayoutTest, SafeDestruction) {
  views::View* child_view = new views::View();
  layout_->AddView(ThreeViewLayout::Container::END, child_view);
  host_.reset();
}

TEST_F(ThreeViewLayoutTest, PaddingBetweenContainers) {
  const int kPaddingBetweenContainers = 3;
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kViewSize(kViewWidth, kViewHeight);
  const int kStartChildExpectedX = 0;
  const int kCenterChildExpectedX =
      kStartChildExpectedX + kViewWidth + kPaddingBetweenContainers;
  const int kEndChildExpectedX =
      kCenterChildExpectedX + kViewWidth + kPaddingBetweenContainers;
  layout_ = new ThreeViewLayout(kPaddingBetweenContainers);
  host_->SetLayoutManager(layout_);

  host_->SetBounds(0, 0, 100, 10);
  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  host_->Layout();

  EXPECT_EQ(kStartChildExpectedX, GetBoundsInHost(start_child).x());
  EXPECT_EQ(kCenterChildExpectedX, GetBoundsInHost(center_child).x());
  EXPECT_EQ(kEndChildExpectedX, GetBoundsInHost(end_child).x());
}

TEST_F(ThreeViewLayoutTest, VerticalOrientation) {
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kViewSize(kViewWidth, kViewHeight);

  layout_ = new ThreeViewLayout(ThreeViewLayout::Orientation::VERTICAL);
  host_->SetLayoutManager(layout_);

  host_->SetBounds(0, 0, 10, 100);
  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  host_->Layout();

  EXPECT_EQ(0, GetBoundsInHost(start_child).y());
  EXPECT_EQ(kViewWidth, GetBoundsInHost(center_child).y());
  EXPECT_EQ(kViewWidth * 2, GetBoundsInHost(end_child).y());
}

TEST_F(ThreeViewLayoutTest, ContainerViewsRemovedFromHost) {
  EXPECT_NE(0, host_->child_count());
  host_->SetLayoutManager(nullptr);
  EXPECT_EQ(0, host_->child_count());
}

TEST_F(ThreeViewLayoutTest, MinCrossAxisSize) {
  const int kMinCrossAxisSize = 15;
  EXPECT_EQ(0, layout_->GetPreferredSize(host_.get()).height());
  layout_->SetMinCrossAxisSize(kMinCrossAxisSize);
  EXPECT_EQ(kMinCrossAxisSize, layout_->GetPreferredSize(host_.get()).height());
  EXPECT_EQ(kMinCrossAxisSize,
            layout_->GetPreferredHeightForWidth(host_.get(), 0));
}

TEST_F(ThreeViewLayoutTest, MainAxisMinSize) {
  host_->SetBounds(0, 0, 100, 10);
  const gfx::Size kMinSize(15, 10);
  layout_->SetMinSize(ThreeViewLayout::Container::START, kMinSize);
  views::View* child = new views::StaticSizedView(gfx::Size(10, 10));
  layout_->AddView(ThreeViewLayout::Container::CENTER, child);
  host_->Layout();

  EXPECT_EQ(kMinSize.width(), GetBoundsInHost(child).x());
}

TEST_F(ThreeViewLayoutTest, MainAxisMaxSize) {
  host_->SetBounds(0, 0, 100, 10);
  const gfx::Size kMaxSize(10, 10);

  layout_->SetMaxSize(ThreeViewLayout::Container::START, kMaxSize);
  views::View* start_child = new views::StaticSizedView(gfx::Size(20, 20));
  layout_->AddView(ThreeViewLayout::Container::START, start_child);

  views::View* center_child = new views::StaticSizedView(gfx::Size(10, 10));
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);

  host_->Layout();

  EXPECT_EQ(kMaxSize.width(), GetBoundsInHost(center_child).x());
}

TEST_F(ThreeViewLayoutTest, ViewsAddedToCorrectContainers) {
  views::View* start_child = new views::StaticSizedView();
  views::View* center_child = new views::StaticSizedView();
  views::View* end_child = new views::StaticSizedView();

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::START)->Contains(start_child));
  EXPECT_EQ(1, GetContainer(ThreeViewLayout::Container::START)->child_count());

  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::CENTER)->Contains(center_child));
  EXPECT_EQ(1, GetContainer(ThreeViewLayout::Container::CENTER)->child_count());

  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::END)->Contains(end_child));
  EXPECT_EQ(1, GetContainer(ThreeViewLayout::Container::END)->child_count());
}

TEST_F(ThreeViewLayoutTest, MultipleViewsAddedToTheSameContainer) {
  views::View* child1 = new views::StaticSizedView();
  views::View* child2 = new views::StaticSizedView();

  layout_->AddView(ThreeViewLayout::Container::START, child1);
  layout_->AddView(ThreeViewLayout::Container::START, child2);

  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::START)->Contains(child1));
  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::START)->Contains(child2));
}

TEST_F(ThreeViewLayoutTest, ViewsRemovedOnSetViewToTheSameContainer) {
  views::View* child1 = new views::StaticSizedView();
  views::View* child2 = new views::StaticSizedView();

  layout_->AddView(ThreeViewLayout::Container::START, child1);
  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::START)->Contains(child1));

  layout_->SetView(ThreeViewLayout::Container::START, child2);
  EXPECT_TRUE(
      GetContainer(ThreeViewLayout::Container::START)->Contains(child2));
  EXPECT_EQ(1, GetContainer(ThreeViewLayout::Container::START)->child_count());
}

TEST_F(ThreeViewLayoutTest, Insets) {
  const int kInset = 3;
  const int kViewHeight = 10;
  const int kExpectedViewHeight = kViewHeight - 2 * kInset;
  const gfx::Size kStartViewSize(10, kViewHeight);
  const gfx::Size kCenterViewSize(100, kViewHeight);
  const gfx::Size kEndViewSize(10, kViewHeight);
  const int kHostWidth = 100;

  host_->SetBounds(0, 0, kHostWidth, kViewHeight);
  layout_->SetInsets(gfx::Insets(kInset));
  views::View* start_child = new views::StaticSizedView(kStartViewSize);
  views::View* center_child = new views::StaticSizedView(kCenterViewSize);
  views::View* end_child = new views::StaticSizedView(kEndViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  layout_->SetFlexForContainer(ThreeViewLayout::Container::CENTER, 1.f);
  host_->Layout();

  EXPECT_EQ(
      gfx::Rect(kInset, kInset, kStartViewSize.width(), kExpectedViewHeight),
      GetBoundsInHost(start_child));
  EXPECT_EQ(gfx::Rect(kInset + kStartViewSize.width(), kInset,
                      kHostWidth - kStartViewSize.width() -
                          kEndViewSize.width() - 2 * kInset,
                      kExpectedViewHeight),
            GetBoundsInHost(center_child));
  EXPECT_EQ(gfx::Rect(kHostWidth - kEndViewSize.width() - kInset, kInset,
                      kEndViewSize.width(), kExpectedViewHeight),
            GetBoundsInHost(end_child));
}

TEST_F(ThreeViewLayoutTest, InvisibleContainerDoesntTakeUpSpace) {
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kViewSize(kViewWidth, kViewHeight);

  host_->SetBounds(0, 0, 30, 10);
  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  layout_->SetContainerVisible(ThreeViewLayout::Container::START, false);
  host_->Layout();

  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), GetBoundsInHost(start_child));
  EXPECT_EQ(0, GetBoundsInHost(center_child).x());
  EXPECT_EQ(kViewWidth, GetBoundsInHost(end_child).x());

  layout_->SetContainerVisible(ThreeViewLayout::Container::START, true);
  host_->Layout();

  EXPECT_EQ(0, GetBoundsInHost(start_child).x());
  EXPECT_EQ(kViewWidth, GetBoundsInHost(center_child).x());
  EXPECT_EQ(kViewWidth * 2, GetBoundsInHost(end_child).x());
}

TEST_F(ThreeViewLayoutTest, NonZeroFlex) {
  const int kHostWidth = 100;
  const gfx::Size kDefaultViewSize(10, 10);
  const gfx::Size kCenterViewSize(100, 10);
  const gfx::Size kExpectedCenterViewSize(
      kHostWidth - 2 * kDefaultViewSize.width(), 10);
  host_->SetBounds(0, 0, kHostWidth, 10);
  views::View* start_child = new views::StaticSizedView(kDefaultViewSize);
  views::View* center_child = new views::StaticSizedView(kCenterViewSize);
  views::View* end_child = new views::StaticSizedView(kDefaultViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  layout_->SetFlexForContainer(ThreeViewLayout::Container::CENTER, 1.f);
  host_->Layout();

  EXPECT_EQ(kDefaultViewSize, GetBoundsInHost(start_child).size());
  EXPECT_EQ(kExpectedCenterViewSize, GetBoundsInHost(center_child).size());
  EXPECT_EQ(kDefaultViewSize, GetBoundsInHost(end_child).size());
}

TEST_F(ThreeViewLayoutTest, NonZeroFlexTakesPrecedenceOverMinSize) {
  const int kHostWidth = 25;
  const gfx::Size kViewSize(10, 10);
  const gfx::Size kMinCenterSize = kViewSize;
  const gfx::Size kExpectedCenterSize(kHostWidth - 2 * kViewSize.width(), 10);
  host_->SetBounds(0, 0, kHostWidth, 10);
  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  layout_->SetFlexForContainer(ThreeViewLayout::Container::CENTER, 1.f);
  layout_->SetMinSize(ThreeViewLayout::Container::CENTER, kMinCenterSize);
  host_->Layout();

  EXPECT_EQ(kViewSize, GetBoundsInHost(start_child).size());
  EXPECT_EQ(
      kExpectedCenterSize,
      GetBoundsInHost(GetContainer(ThreeViewLayout::Container::CENTER)).size());
  EXPECT_EQ(kViewSize, GetBoundsInHost(end_child).size());
}

TEST_F(ThreeViewLayoutTest, NonZeroFlexTakesPrecedenceOverMaxSize) {
  const int kHostWidth = 100;
  const gfx::Size kViewSize(10, 10);
  const gfx::Size kMaxCenterSize(20, 10);
  const gfx::Size kExpectedCenterSize(kHostWidth - 2 * kViewSize.width(), 10);
  host_->SetBounds(0, 0, kHostWidth, 10);
  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  layout_->AddView(ThreeViewLayout::Container::START, start_child);
  layout_->AddView(ThreeViewLayout::Container::CENTER, center_child);
  layout_->AddView(ThreeViewLayout::Container::END, end_child);

  layout_->SetFlexForContainer(ThreeViewLayout::Container::CENTER, 1.f);
  layout_->SetMaxSize(ThreeViewLayout::Container::CENTER, kMaxCenterSize);
  host_->Layout();

  EXPECT_EQ(kViewSize, GetBoundsInHost(start_child).size());
  EXPECT_EQ(
      kExpectedCenterSize,
      GetBoundsInHost(GetContainer(ThreeViewLayout::Container::CENTER)).size());
  EXPECT_EQ(kViewSize, GetBoundsInHost(end_child).size());
}

}  // namespace ash
