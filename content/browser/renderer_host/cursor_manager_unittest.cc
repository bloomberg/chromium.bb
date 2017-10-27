// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cursor_manager.h"

#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/cursors/webcursor.h"
#include "content/public/common/cursor_info.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/dummy_render_widget_host_delegate.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

// CursorManager is only instantiated on Aura and Mac.
#if defined(USE_AURA) || defined(OS_MACOSX)

namespace content {

namespace {

class MockRenderWidgetHostViewForCursors : public TestRenderWidgetHostView {
 public:
  MockRenderWidgetHostViewForCursors(RenderWidgetHost* host, bool top_view)
      : TestRenderWidgetHostView(host) {
    if (top_view)
      cursor_manager_.reset(new CursorManager(this));
  }

  void DisplayCursor(const WebCursor& cursor) override {
    current_cursor_ = cursor;
  }

  CursorManager* GetCursorManager() override { return cursor_manager_.get(); }

  WebCursor cursor() { return current_cursor_; }

 private:
  WebCursor current_cursor_;
  std::unique_ptr<CursorManager> cursor_manager_;
};

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  static MockRenderWidgetHost* Create(RenderWidgetHostDelegate* delegate,
                                      RenderProcessHost* process,
                                      int32_t routing_id) {
    mojom::WidgetPtr widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    return new MockRenderWidgetHost(delegate, process, routing_id,
                                    std::move(widget_impl), std::move(widget));
  }

 private:
  MockRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id,
                       std::unique_ptr<MockWidgetImpl> widget_impl,
                       mojom::WidgetPtr widget)
      : RenderWidgetHostImpl(delegate,
                             process,
                             routing_id,
                             std::move(widget),
                             false),
        widget_impl_(std::move(widget_impl)) {}

  std::unique_ptr<MockWidgetImpl> widget_impl_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

class CursorManagerTest : public testing::Test {
 public:
  CursorManagerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    process_host_.reset(new MockRenderProcessHost(browser_context_.get()));
    widget_host_.reset(MakeNewWidgetHost());
    top_view_ =
        new MockRenderWidgetHostViewForCursors(widget_host_.get(), true);
  }

  RenderWidgetHostImpl* MakeNewWidgetHost() {
    int32_t routing_id = process_host_->GetNextRoutingID();
    return MockRenderWidgetHost::Create(&delegate_, process_host_.get(),
                                        routing_id);
  }

  void TearDown() override {
    if (top_view_)
      delete top_view_;

    widget_host_.reset();
    process_host_.reset();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;

  // Tests should set this to nullptr if they've already triggered its
  // destruction.
  MockRenderWidgetHostViewForCursors* top_view_;

  DummyRenderWidgetHostDelegate delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorManagerTest);
};

}  // namespace

// Verify basic CursorManager functionality when no OOPIFs are present.
TEST_F(CursorManagerTest, CursorOnSingleView) {
  // Simulate mouse over the top-level frame without an UpdateCursor message.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(top_view_);

  // The view should be using the default cursor.
  EXPECT_TRUE(top_view_->cursor().IsEqual(WebCursor()));

  CursorInfo cursor_info(blink::WebCursorInfo::kTypeHand);
  WebCursor cursor_hand;
  cursor_hand.InitFromCursorInfo(cursor_info);

  // Update the view with a non-default cursor.
  top_view_->GetCursorManager()->UpdateCursor(top_view_, cursor_hand);

  // Verify the RenderWidgetHostView now uses the correct cursor.
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_hand));
}

// Verify cursor interactions between a parent frame and an out-of-process
// child frame.
TEST_F(CursorManagerTest, CursorOverChildView) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view(
      new MockRenderWidgetHostViewForCursors(widget_host.get(), false));

  CursorInfo cursor_info(blink::WebCursorInfo::kTypeHand);
  WebCursor cursor_hand;
  cursor_hand.InitFromCursorInfo(cursor_info);

  // Set the child frame's cursor to a hand. This should not propagate to the
  // top-level view without the mouse moving over the child frame.
  top_view_->GetCursorManager()->UpdateCursor(child_view.get(), cursor_hand);
  EXPECT_FALSE(top_view_->cursor().IsEqual(cursor_hand));

  // Now moving the mouse over the child frame should update the overall cursor.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view.get());
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_hand));

  // Destruction of the child view should restore the parent frame's cursor.
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view.get());
  EXPECT_FALSE(top_view_->cursor().IsEqual(cursor_hand));
}

// Verify interactions between two independent OOPIFs, including interleaving
// cursor updates and mouse movements. This simulates potential race
// conditions between cursor updates.
TEST_F(CursorManagerTest, CursorOverMultipleChildViews) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host1(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view1(
      new MockRenderWidgetHostViewForCursors(widget_host1.get(), false));
  std::unique_ptr<RenderWidgetHostImpl> widget_host2(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view2(
      new MockRenderWidgetHostViewForCursors(widget_host2.get(), false));

  CursorInfo cursor_info_hand(blink::WebCursorInfo::kTypeHand);
  WebCursor cursor_hand;
  cursor_hand.InitFromCursorInfo(cursor_info_hand);

  CursorInfo cursor_info_cross(blink::WebCursorInfo::kTypeCross);
  WebCursor cursor_cross;
  cursor_cross.InitFromCursorInfo(cursor_info_cross);

  CursorInfo cursor_info_pointer(blink::WebCursorInfo::kTypePointer);
  WebCursor cursor_pointer;
  cursor_pointer.InitFromCursorInfo(cursor_info_pointer);

  // Initialize each View to a different cursor.
  top_view_->GetCursorManager()->UpdateCursor(top_view_, cursor_hand);
  top_view_->GetCursorManager()->UpdateCursor(child_view1.get(), cursor_cross);
  top_view_->GetCursorManager()->UpdateCursor(child_view2.get(),
                                              cursor_pointer);
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_hand));

  // Simulate moving the mouse between child views and receiving cursor updates.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view1.get());
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_cross));
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view2.get());
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_pointer));

  // Simulate cursor updates to both child views and the parent view. An
  // update to child_view1 or the parent view should not change the current
  // cursor because the mouse is over child_view2.
  top_view_->GetCursorManager()->UpdateCursor(child_view1.get(), cursor_hand);
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_pointer));
  top_view_->GetCursorManager()->UpdateCursor(child_view2.get(), cursor_cross);
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_cross));
  top_view_->GetCursorManager()->UpdateCursor(top_view_, cursor_hand);
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_cross));

  // Similarly, destroying child_view1 should have no effect on the cursor,
  // but destroying child_view2 should change it.
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view1.get());
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_cross));
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view2.get());
  EXPECT_TRUE(top_view_->cursor().IsEqual(cursor_hand));
}

}  // namespace content

#endif  // defined(USE_AURA) || defined(OS_MACOSX)
