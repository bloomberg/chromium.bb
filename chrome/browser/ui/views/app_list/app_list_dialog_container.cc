// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/app_list_dialog_container.h"

#include "base/callback.h"
#include "chrome/browser/ui/host_desktop.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/resources/grit/ui_resources.h"
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

AppListDialogContainer::AppListDialogContainer(
    views::View* dialog_body,
    const base::Closure& close_callback)
    : dialog_body_(dialog_body),
      close_callback_(close_callback),
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

AppListDialogContainer::~AppListDialogContainer() {
}

void AppListDialogContainer::Layout() {
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

views::View* AppListDialogContainer::GetInitiallyFocusedView() {
  return GetContentsView();
}

views::View* AppListDialogContainer::GetContentsView() {
  return this;
}

views::ClientView* AppListDialogContainer::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, GetContentsView());
}

views::NonClientFrameView* AppListDialogContainer::CreateNonClientFrameView(
    views::Widget* widget) {
  return new views::NativeFrameView(widget);
}

void AppListDialogContainer::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == close_button_) {
    GetWidget()->Close();
  } else {
    NOTREACHED();
  }
}

ui::ModalType AppListDialogContainer::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void AppListDialogContainer::WindowClosing() {
  if (!close_callback_.is_null())
    close_callback_.Run();
}
