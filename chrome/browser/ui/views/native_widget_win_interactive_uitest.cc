// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/view_event_test_base.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

class NativeWidgetWinTest : public ViewEventTestBase {
 public:
  NativeWidgetWinTest()
      : ViewEventTestBase(),
        contents_view_(new views::View) {
  }

  virtual void SetUp() OVERRIDE {
    local_state_.reset(new ScopedTestingLocalState(
        TestingBrowserProcess::GetGlobal()));
    ViewEventTestBase::SetUp();
  }

 protected:
  virtual views::View* CreateContentsView() OVERRIDE {
    return contents_view_;
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(500, 500);
  }

  views::View* contents_view_;

  scoped_ptr<views::Widget> child_;

 private:
  scoped_ptr<ScopedTestingLocalState> local_state_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetWinTest);
};

// This test creates a child Widget that is attached by way of a NativeViewHost.
// The child HWND is focused (through NativeViewHost) the top level Widget is
// minimized then restored and the test asserts focus is still on the child
// widget and corresponding NativeViewHost.
//
// This scenario replicates what happens with a browser and a corresponding
// RenderWidgetHostViewWin.
//
// This test really belongs in views, but as it exercises focus it needs to
// be here.
class NativeWidgetWinTest1 : public NativeWidgetWinTest {
 public:
  NativeWidgetWinTest1()
      : NativeWidgetWinTest(),
        child_(NULL),
        native_view_host_(NULL) {
  }

  virtual void DoTestOnMessageLoop() OVERRIDE {
    AttachChildWidget();
    // Focus the native widget.
    native_view_host_->RequestFocus();
    EXPECT_TRUE(native_view_host_->HasFocus());
    EXPECT_EQ(child_->GetNativeView(), ::GetFocus());

    // Minimize then restore the window, focus should still be on the
    // NativeViewHost and its corresponding widget.
    window_->Minimize();
    DLOG(WARNING) << "Restoring widget=" << window_->GetNativeView();
    window_->Restore();
    EXPECT_TRUE(native_view_host_->HasFocus());
    EXPECT_EQ(child_->GetNativeView(), ::GetFocus());

    Done();
  }

 private:
  void AttachChildWidget() {
    child_ = new views::Widget();
    views::Widget::InitParams child_params(
        views::Widget::InitParams::TYPE_CONTROL);
    child_params.parent = window_->GetNativeView();
    child_->Init(child_params);

    native_view_host_ = new views::NativeViewHost;
    native_view_host_->set_focusable(true);
    contents_view_->AddChildView(native_view_host_);
    native_view_host_->SetBoundsRect(gfx::Rect(0, 0, 200, 200));
    native_view_host_->Attach(child_->GetNativeView());
  }

  // Owned by the parent widget.
  views::Widget* child_;

  // Owned by the parent View.
  views::NativeViewHost* native_view_host_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetWinTest1);
};

VIEW_TEST(NativeWidgetWinTest1, FocusRestoredToChildAfterMiniminize)
