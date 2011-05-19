// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include "views/widget/native_widget_gtk.h"

using views::NativeWidgetGtk;

namespace chromeos {

namespace {

class ControlsWidget : public NativeWidgetGtk {
 public:
  ControlsWidget() : NativeWidgetGtk(new views::Widget) {
  }

 private:
  // NativeWidgetGtk overrides:
  virtual void OnMap(GtkWidget* widget) OVERRIDE {
    // For some reason, Controls window never gets first expose event,
    // which makes WM believe that the login screen is not ready.
    // This is a workaround to let WM show the login screen. While
    // this may allow WM to show unpainted window, we haven't seen any
    // issue (yet). We will not investigate this further because we're
    // migrating to different implemention (WebUI).
    UpdateFreezeUpdatesProperty(GTK_WINDOW(GetNativeView()),
                                false /* remove */);
  }

  DISALLOW_COPY_AND_ASSIGN(ControlsWidget);
};

// Widget that notifies window manager about clicking on itself.
// Doesn't send anything if user is selected.
class ClickNotifyingWidget : public NativeWidgetGtk {
 public:
  explicit ClickNotifyingWidget(UserController* controller)
      : NativeWidgetGtk(new views::Widget),
        controller_(controller) {
  }

 private:
  gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
    if (!controller_->IsUserSelected())
      controller_->SelectUserRelative(0);

    return NativeWidgetGtk::OnButtonPress(widget, event);
  }

  UserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ClickNotifyingWidget);
};

views::Widget* InitWidget(views::NativeWidget* native_widget,
                          const gfx::Rect& bounds) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.transparent = true;
  params.bounds = bounds;
  params.native_widget = native_widget;
  native_widget->GetWidget()->Init(params);
  GdkWindow* gdk_window = native_widget->GetWidget()->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);
  return native_widget->GetWidget();
}

}  // namespace

// static
views::Widget* UserController::CreateControlsWidget(const gfx::Rect& bounds) {
  return InitWidget(new ControlsWidget(), bounds);
}

// static
views::Widget* UserController::CreateClickNotifyingWidget(
    UserController* controller,
    const gfx::Rect& bounds) {
  return InitWidget(new ClickNotifyingWidget(controller), bounds);
}

}  // namespace
