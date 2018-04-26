// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    content::PictureInPictureWindowController* controller) {
  return base::WrapUnique(new OverlayWindowViews(controller));
}

namespace {
const int kBorderThickness = 5;
const int kResizeAreaCornerSize = 16;

// TODO(apacible): Update sizes per UX feedback and when scaling is determined.
// http://crbug.com/836389
const gfx::Size kCloseIconSize = gfx::Size(50, 50);
const gfx::Size kPlayPauseIconSize = gfx::Size(120, 120);
}  // namespace

// OverlayWindow implementation of NonClientFrameView.
class OverlayWindowFrameView : public views::NonClientFrameView {
 public:
  OverlayWindowFrameView() = default;
  ~OverlayWindowFrameView() override = default;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // Allow dragging the border of the window to resize. Within the bounds of
    // the window, allow dragging to reposition the window.
    int window_component = GetHTComponentForFrame(
        point, kBorderThickness, kBorderThickness, kResizeAreaCornerSize,
        kResizeAreaCornerSize, GetWidget()->widget_delegate()->CanResize());
    return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayWindowFrameView);
};

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
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_NONE; }
  base::string16 GetWindowTitle() const override {
    // While the window title is not shown on the window itself, it is used to
    // identify the window on the system tray.
    return l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_TITLE_TEXT);
  }
  bool ShouldShowWindowTitle() const override { return false; }
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new OverlayWindowFrameView();
  }

 private:
  // Owns OverlayWindowWidgetDelegate.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowWidgetDelegate);
};

OverlayWindowViews::OverlayWindowViews(
    content::PictureInPictureWindowController* controller)
    : controller_(controller),
      video_view_(new views::View()),
      close_controls_view_(new views::ImageButton(nullptr)),
      play_pause_controls_view_(new views::ToggleImageButton(nullptr)) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = CalculateAndUpdateBounds();
  params.keep_on_top = true;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;

  // Set WidgetDelegate for more control over |widget_|.
  params.delegate = new OverlayWindowWidgetDelegate(this);

  Init(params);
  SetUpViews();
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
  // Once |current_size_| is calculated at least once, it should stay within the
  // bounds of |min_size_| and |max_size_|.
  if (current_size_.IsEmpty()) {
    current_size_ = gfx::Size(work_area.width() / 5, work_area.height() / 5);
    current_size_.set_width(std::min(
        max_size_.width(), std::max(min_size_.width(), current_size_.width())));
    current_size_.set_height(
        std::min(max_size_.height(),
                 std::max(min_size_.height(), current_size_.height())));
  }

  // Determine the window size by fitting |natural_size_| within
  // |current_size_|, keeping to |natural_size_|'s aspect ratio.
  if (!natural_size_.IsEmpty())
    current_size_ =
        media::ScaleSizeToFitWithinTarget(natural_size_, current_size_);

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

void OverlayWindowViews::SetUpViews() {
  // Set up views::View that closes the window.
  close_controls_view_->SetSize(kCloseIconSize);
  close_controls_view_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                          views::ImageButton::ALIGN_MIDDLE);
  close_controls_view_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kPictureInPictureCloseIcon, SK_ColorWHITE));

  // Set up views::View that toggles play/pause.
  play_pause_controls_view_->SetSize(kPlayPauseIconSize);
  play_pause_controls_view_->SetImageAlignment(
      views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
  play_pause_controls_view_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kPictureInPicturePlayIcon,
                            kPlayPauseIconSize.width(), SK_ColorWHITE));
  gfx::ImageSkia pause_icon =
      gfx::CreateVectorIcon(views::kPictureInPicturePauseIcon,
                            kPlayPauseIconSize.width(), SK_ColorWHITE);
  play_pause_controls_view_->SetToggledImage(views::Button::STATE_NORMAL,
                                             &pause_icon);
  play_pause_controls_view_->SetToggled(false);

  // Paint to ui::Layers to use in the OverlaySurfaceEmbedder.
  video_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  play_pause_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);

  // Don't show the controls until the mouse hovers over the window.
  GetCloseControlsLayer()->SetVisible(false);
  GetPlayPauseControlsLayer()->SetVisible(false);
}

bool OverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void OverlayWindowViews::Close() {
  views::Widget::Close();
}

void OverlayWindowViews::Show() {
  views::Widget::Show();
}

void OverlayWindowViews::Hide() {
  views::Widget::Hide();
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

void OverlayWindowViews::UpdateVideoSize(const gfx::Size& natural_size) {
  DCHECK(!natural_size.IsEmpty());
  natural_size_ = natural_size;

  // Update the views::Widget bounds to adhere to sizing spec.
  SetBounds(CalculateAndUpdateBounds());
}

ui::Layer* OverlayWindowViews::GetVideoLayer() {
  return video_view_->layer();
}

ui::Layer* OverlayWindowViews::GetCloseControlsLayer() {
  return close_controls_view_->layer();
}

ui::Layer* OverlayWindowViews::GetPlayPauseControlsLayer() {
  return play_pause_controls_view_->layer();
}

gfx::Rect OverlayWindowViews::GetCloseControlsBounds() {
  return gfx::Rect(
      gfx::Point(GetBounds().size().width() - kCloseIconSize.width(), 0),
      kCloseIconSize);
}

gfx::Rect OverlayWindowViews::GetPlayPauseControlsBounds() {
  return gfx::Rect(
      gfx::Point(
          (GetBounds().size().width() - kPlayPauseIconSize.width()) / 2,
          (GetBounds().size().height() - kPlayPauseIconSize.height()) / 2),
      kPlayPauseIconSize);
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

void OverlayWindowViews::OnMouseEvent(ui::MouseEvent* event) {
  // TODO(apacible): Handle tab focusing and touch screen events.
  // http://crbug.com/836389
  switch (event->type()) {
    // Only show the media controls when the mouse is hovering over the window.
    case ui::ET_MOUSE_ENTERED:
      GetCloseControlsLayer()->SetVisible(true);
      GetPlayPauseControlsLayer()->SetVisible(true);
      break;

    case ui::ET_MOUSE_EXITED:
      GetCloseControlsLayer()->SetVisible(false);
      GetPlayPauseControlsLayer()->SetVisible(false);
      break;

    case ui::ET_MOUSE_RELEASED:
      if (!event->IsOnlyLeftMouseButton())
        return;

      // TODO(apacible): Clip the clickable areas to where the button icons are
      // drawn. http://crbug.com/836389
      if (GetCloseControlsBounds().Contains(event->location())) {
        controller_->Close();
        event->SetHandled();
      } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
        bool is_active = controller_->TogglePlayPause();
        play_pause_controls_view_->SetToggled(is_active);
        event->SetHandled();
      }
      break;

    default:
      break;
  }
}

void OverlayWindowViews::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  // Update the surface layer bounds to stretch / shrink the size of the shown
  // video in the window.
  if (controller_)
    controller_->UpdateLayerBounds();

  views::Widget::OnNativeWidgetSizeChanged(new_size);
}
