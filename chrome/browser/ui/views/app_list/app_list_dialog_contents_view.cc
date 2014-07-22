// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/app_list_dialog_contents_view.h"

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/host_desktop.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/views/window/non_client_view.h"

namespace {

// Margin of the close button from the top right-hand corner of the dialog.
const int kCloseButtonDialogMargin = 10;

// The radius of the app list overlay (the dialog's background).
// TODO(sashab): Using SupportsShadow() from app_list_view.cc, make this
// 1px smaller on platforms that support shadows.
const int kAppListOverlayBorderRadius = 3;

// The background for App List dialogs, which appears as a rounded rectangle
// with the same border radius and color as the app list contents.
class AppListOverlayBackground : public views::Background {
 public:
  AppListOverlayBackground() {}
  virtual ~AppListOverlayBackground() {}

  // Overridden from views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(app_list::kContentsBackgroundColor);
    canvas->DrawRoundRect(
        view->GetContentsBounds(), kAppListOverlayBorderRadius, paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListOverlayBackground);
};

}  // namespace

AppListDialogContentsView::AppListDialogContentsView(
    AppListControllerDelegate* app_list_controller_delegate,
    views::View* dialog_body)
    : app_list_controller_delegate_(app_list_controller_delegate),
      dialog_body_(dialog_body),
      close_button_(NULL) {
  set_background(new AppListOverlayBackground());
  AddChildView(dialog_body_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  close_button_ = new views::LabelButton(this, base::string16());
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          *rb.GetImageNamed(IDR_CLOSE_DIALOG).ToImageSkia());
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                          *rb.GetImageNamed(IDR_CLOSE_DIALOG_H).ToImageSkia());
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                          *rb.GetImageNamed(IDR_CLOSE_DIALOG_P).ToImageSkia());
  close_button_->SetBorder(views::Border::NullBorder());

  // Bind the ESCAPE key to trigger selecting the close button.
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  close_button_->AddAccelerator(escape);

  AddChildView(close_button_);
}

AppListDialogContentsView::~AppListDialogContentsView() {
}

// static
views::Widget* AppListDialogContentsView::CreateDialogWidget(
    gfx::NativeView parent,
    const gfx::Rect& bounds,
    AppListDialogContentsView* dialog) {
  views::Widget::InitParams params;
  params.delegate = dialog;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.remove_standard_frame = true;
  params.parent = parent;
  params.bounds = bounds;
  // Since this stretches to the corners of the app list, any shadow we add
  // would be hidden anyway, so don't add one.
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  // TODO(sashab): Plumb wm::WindowVisibilityAnimationType through to the
  // NativeWindow so that widgets can specify custom animations for their
  // windows. Then specify a matching animation for this type of widget,
  // such as WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL.

  views::Widget* widget = new views::Widget();
  widget->Init(params);
  return widget;
}

void AppListDialogContentsView::Layout() {
  // Place the close button in the top right-hand corner.
  close_button_->SetSize(close_button_->GetPreferredSize());
  close_button_->SetPosition(
      gfx::Point(width() - close_button_->width() - kCloseButtonDialogMargin,
                 kCloseButtonDialogMargin));

  // Stretch the dialog body to fill the whole dialog area.
  dialog_body_->SetBoundsRect(GetContentsBounds());

  // Propagate Layout() calls to subviews.
  views::View::Layout();
}

views::View* AppListDialogContentsView::GetInitiallyFocusedView() {
  return GetContentsView();
}

views::View* AppListDialogContentsView::GetContentsView() {
  return this;
}

views::ClientView* AppListDialogContentsView::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, GetContentsView());
}

views::NonClientFrameView* AppListDialogContentsView::CreateNonClientFrameView(
    views::Widget* widget) {
  return new views::NativeFrameView(widget);
}

void AppListDialogContentsView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender == close_button_) {
    GetWidget()->Close();
  } else {
    NOTREACHED();
  }
}

ui::ModalType AppListDialogContentsView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void AppListDialogContentsView::WindowClosing() {
  app_list_controller_delegate_->OnCloseChildDialog();
}
