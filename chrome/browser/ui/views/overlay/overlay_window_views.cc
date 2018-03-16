// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget_delegate.h"

// static
std::unique_ptr<OverlayWindow> OverlayWindow::Create() {
  return base::WrapUnique(new OverlayWindowViews());
}

// OverlayWindow implementation of WidgetDelegate.
class OverlayWindowWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit OverlayWindowWidgetDelegate(views::Widget* widget)
      : widget_(widget) {
    DCHECK(widget_);
  }
  ~OverlayWindowWidgetDelegate() override = default;

  // views::WidgetDelegate:
  bool CanResize() const override { return true; }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }
  base::string16 GetWindowTitle() const override {
    return l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_TITLE_TEXT);
  }
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }

 private:
  // Owns OverlayWindowWidgetDelegate.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowWidgetDelegate);
};

OverlayWindowViews::OverlayWindowViews() {
  // TODO(apacible): Change window type to TYPE_WINDOW_FRAMELESS. It is
  // temporarily TYPE_WINDOW for resizing purposes.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = CalculateAndUpdateBounds();
  params.keep_on_top = true;
  params.visible_on_all_workspaces = true;

  // Set WidgetDelegate for more control over |widget_|.
  params.delegate = new OverlayWindowWidgetDelegate(this);

  Init(params);
}

OverlayWindowViews::~OverlayWindowViews() = default;

gfx::Rect OverlayWindowViews::CalculateAndUpdateBounds() {
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // Upper bound size of the window is 50% of the display width and height.
  max_size_ = gfx::Size(work_area.width() / 2, work_area.height() / 2);

  // Lower bound size of the window is a fixed value to allow for minimal sizes
  // on UI affordances, such as buttons. This is currently a placeholder value.
  min_size_ = gfx::Size(144, 100);

  // Initial size of the window is always 20% of the display width and height,
  // constrained by the min and max sizes. Only explicitly update this the first
  // time |current_size_| is being calculated.
  if (current_size_.IsEmpty())
    current_size_ = gfx::Size(work_area.width() / 5, work_area.height() / 5);

  // TODO(apacible): Take into account the video aspect ratio.
  current_size_.set_width(std::min(
      max_size_.width(), std::max(min_size_.width(), current_size_.width())));
  current_size_.set_height(
      std::min(max_size_.height(),
               std::max(min_size_.height(), current_size_.height())));

  // The initial positioning is on the bottom right quadrant
  // of the primary display work area.
  int window_diff_width = work_area.width() - current_size_.width();
  int window_diff_height = work_area.height() - current_size_.height();

  // Keep a margin distance of 2% the average of the two window size
  // differences, keeping the margins consistent.
  int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;
  return gfx::Rect(
      gfx::Point(window_diff_width - buffer, window_diff_height - buffer),
      current_size_);
}

bool OverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void OverlayWindowViews::Show() {
  views::Widget::Show();
}

void OverlayWindowViews::Close() {
  views::Widget::Close();
}

bool OverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

bool OverlayWindowViews::IsAlwaysOnTop() const {
  return true;
}

ui::Layer* OverlayWindowViews::GetLayer() {
  return views::Widget::GetLayer();
}

gfx::Rect OverlayWindowViews::GetBounds() const {
  return views::Widget::GetRestoredBounds();
}

gfx::Size OverlayWindowViews::GetMinimumSize() const {
  return min_size_;
}

gfx::Size OverlayWindowViews::GetMaximumSize() const {
  return max_size_;
}

void OverlayWindowViews::OnNativeWidgetWorkspaceChanged() {
  // TODO(apacible): Update sizes and maybe resize the current
  // Picture-in-Picture window. Currently, switching between workspaces on linux
  // does not trigger this function. http://crbug.com/819673
}
