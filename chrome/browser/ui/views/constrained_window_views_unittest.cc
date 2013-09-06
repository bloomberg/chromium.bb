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
      web_modal::WebContentsModalDialogHostObserver* observer) OVERRIDE {};
  virtual void RemoveObserver(
      web_modal::WebContentsModalDialogHostObserver* observer) OVERRIDE {};

 private:
  gfx::NativeView host_view_;
  gfx::Size max_dialog_size_;

  DISALLOW_COPY_AND_ASSIGN(DialogHost);
};

typedef ViewsTestBase ConstrainedWindowViewsTest;

TEST_F(ConstrainedWindowViewsTest, UpdateDialogPosition) {
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  Widget parent;
  parent.Init(params);

  DialogContents* contents = new DialogContents;
  Widget* dialog =
      CreateBrowserModalDialogViews(contents, parent.GetNativeWindow());
  DialogHost dialog_host(parent.GetNativeView());
  UpdateWebContentsModalDialogPosition(dialog, &dialog_host);

  // Set the preferred size to something larger than the size of a dialog with
  // no content.
  gfx::Size preferred_size = dialog->GetClientAreaBoundsInScreen().size();
  preferred_size.Enlarge(50, 50);
  contents->set_preferred_size(preferred_size);
  UpdateWebContentsModalDialogPosition(dialog, &dialog_host);

  // Now increase the preferred content area and make sure the dialog grows by
  // the same amount after the position is updated.
  gfx::Size expected_size = dialog->GetClientAreaBoundsInScreen().size();
  expected_size.Enlarge(200, 200);
  preferred_size.Enlarge(200, 200);
  contents->set_preferred_size(preferred_size);
  UpdateWebContentsModalDialogPosition(dialog, &dialog_host);
  EXPECT_EQ(expected_size, dialog->GetClientAreaBoundsInScreen().size());

  // Make sure the dialog shrinks when the preferred content area shrinks.
  expected_size.Enlarge(-200, -200);
  preferred_size.Enlarge(-200, -200);
  contents->set_preferred_size(preferred_size);
  UpdateWebContentsModalDialogPosition(dialog, &dialog_host);
  EXPECT_EQ(expected_size, dialog->GetClientAreaBoundsInScreen().size());

  // Make sure the dialog is never larger than the max dialog size the dialog
  // host can handle.
  gfx::Size full_dialog_size = dialog->GetClientAreaBoundsInScreen().size();
  gfx::Size max_dialog_size = full_dialog_size;
  max_dialog_size.Enlarge(-100, -100);
  dialog_host.set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog, &dialog_host);
  // The top border of the dialog is intentionally drawn outside the area
  // specified by the dialog host, so add it to the size the dialog is expected
  // to occupy.
  expected_size = max_dialog_size;
  Border* border = dialog->non_client_view()->frame_view()->border();
  if (border)
    expected_size.Enlarge(0, border->GetInsets().top());
  EXPECT_EQ(expected_size,
            dialog->non_client_view()->GetBoundsInScreen().size());

  // Enlarge the max area again and make sure the dialog again uses its
  // preferred size.
  dialog_host.set_max_dialog_size(gfx::Size(5000, 5000));
  UpdateWebContentsModalDialogPosition(dialog, &dialog_host);
  EXPECT_EQ(full_dialog_size, dialog->GetClientAreaBoundsInScreen().size());
}

}  // namespace views
