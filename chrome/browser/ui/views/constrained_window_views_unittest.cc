// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_modal {
class WebContentsModalDialogHostObserver;
}

namespace views {

class DialogContents : public DialogDelegateView {
 public:
  DialogContents() {}
  virtual ~DialogContents() {}

  void set_preferred_size(const gfx::Size& preferred_size) {
    preferred_size_ = preferred_size;
  }

  // Overriden from DialogDelegateView:
  virtual View* GetContentsView() OVERRIDE { return this; }
  virtual gfx::Size GetPreferredSize() OVERRIDE { return preferred_size_; }
  virtual gfx::Size GetMinimumSize() OVERRIDE { return gfx::Size(); }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(DialogContents);
};

class DialogHost : public web_modal::WebContentsModalDialogHost {
 public:
  explicit DialogHost(gfx::NativeView host_view)
      : host_view_(host_view),
        max_dialog_size_(5000, 5000) {
  }

  virtual ~DialogHost() {}

  void set_max_dialog_size(const gfx::Size& max_dialog_size) {
    max_dialog_size_ = max_dialog_size;
  }

  // Overridden from WebContentsModalDialogHost:
  virtual gfx::NativeView GetHostView() const OVERRIDE { return host_view_; }
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE {
    return gfx::Point();
  }
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE { return max_dialog_size_; }
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE {};
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE {};

 private:
  gfx::NativeView host_view_;
  gfx::Size max_dialog_size_;

  DISALLOW_COPY_AND_ASSIGN(DialogHost);
};

class ConstrainedWindowViewsTest : public ViewsTestBase {
 public:
  ConstrainedWindowViewsTest() : contents_(NULL) {}
  virtual ~ConstrainedWindowViewsTest() {}

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();
    contents_ = new DialogContents;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.delegate = contents_;
    dialog_.reset(new Widget);
    dialog_->Init(params);
    dialog_host_.reset(new DialogHost(dialog_->GetNativeView()));

    // Make sure the dialog size is dominated by the preferred size of the
    // contents.
    gfx::Size preferred_size = dialog()->GetRootView()->GetPreferredSize();
    preferred_size.Enlarge(500, 500);
    contents()->set_preferred_size(preferred_size);
  }

  virtual void TearDown() OVERRIDE {
    ViewsTestBase::TearDown();
    contents_ = NULL;
    dialog_host_.reset();
    dialog_.reset();
  }

  gfx::Size GetDialogSize() {
    return dialog()->GetRootView()->GetBoundsInScreen().size();
  }

  DialogContents* contents() { return contents_; }
  DialogHost* dialog_host() { return dialog_host_.get(); }
  Widget* dialog() { return dialog_.get(); }

 private:
  DialogContents* contents_;
  scoped_ptr<DialogHost> dialog_host_;
  scoped_ptr<Widget> dialog_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViewsTest);
};

// Make sure a dialog that increases its preferred size grows on the next
// position update.
TEST_F(ConstrainedWindowViewsTest, GrowModalDialogSize) {
  UpdateBrowserModalDialogPosition(dialog(), dialog_host());
  gfx::Size expected_size = GetDialogSize();
  gfx::Size preferred_size = contents()->GetPreferredSize();
  expected_size.Enlarge(50, 50);
  preferred_size.Enlarge(50, 50);
  contents()->set_preferred_size(preferred_size);
  UpdateBrowserModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());
}

// Make sure a dialog that reduces its preferred size shrinks on the next
// position update.
TEST_F(ConstrainedWindowViewsTest, ShrinkModalDialogSize) {
  UpdateBrowserModalDialogPosition(dialog(), dialog_host());
  gfx::Size expected_size = GetDialogSize();
  gfx::Size preferred_size = contents()->GetPreferredSize();
  expected_size.Enlarge(-50, -50);
  preferred_size.Enlarge(-50, -50);
  contents()->set_preferred_size(preferred_size);
  UpdateBrowserModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());
}

// Make sure browser modal dialogs are not affected by restrictions on web
// content modal dialog maximum sizes.
TEST_F(ConstrainedWindowViewsTest, MaximumBrowserDialogSize) {
  UpdateBrowserModalDialogPosition(dialog(), dialog_host());
  gfx::Size dialog_size = GetDialogSize();
  gfx::Size max_dialog_size = dialog_size;
  max_dialog_size.Enlarge(-50, -50);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateBrowserModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(dialog_size.ToString(), GetDialogSize().ToString());
}

// Web content modal dialogs should not get a size larger than what the dialog
// host gives as the maximum size.
TEST_F(ConstrainedWindowViewsTest, MaximumWebContentsDialogSize) {
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  gfx::Size full_dialog_size = GetDialogSize();
  gfx::Size max_dialog_size = full_dialog_size;
  max_dialog_size.Enlarge(-50, -50);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  // The top border of the dialog is intentionally drawn outside the area
  // specified by the dialog host, so add it to the size the dialog is expected
  // to occupy.
  gfx::Size expected_size = max_dialog_size;
  Border* border = dialog()->non_client_view()->frame_view()->border();
  if (border)
    expected_size.Enlarge(0, border->GetInsets().top());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());

  // Increasing the maximum dialog size should bring the dialog back to its
  // original size.
  max_dialog_size.Enlarge(100, 100);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(full_dialog_size.ToString(), GetDialogSize().ToString());
}

}  // namespace views
