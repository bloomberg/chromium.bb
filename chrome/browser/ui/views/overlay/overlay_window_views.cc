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
const gfx::Size kMinWindowSize = gfx::Size(144, 100);

const int kBorderThickness = 5;
const int kResizeAreaCornerSize = 16;

// TODO(apacible): Update sizes per UX feedback and when scaling is determined.
// http://crbug.com/836389
const gfx::Size kCloseIconSize = gfx::Size(50, 50);
const gfx::Size kPlayPauseIconSize = gfx::Size(90, 90);
}  // namespace

// OverlayWindow implementation of NonClientFrameView.
class OverlayWindowFrameView : public views::NonClientFrameView {
 public:
  explicit OverlayWindowFrameView(views::Widget* widget) : widget_(widget) {}
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

    // Allow dragging the border of the window to resize when interacting with
    // the window outside the control icons.
    OverlayWindowViews* window = static_cast<OverlayWindowViews*>(widget_);
    if (!window->GetCloseControlsBounds().Contains(point) &&
        !window->GetPlayPauseControlsBounds().Contains(point))
      return HTCAPTION;

    // The areas left to interact with are media controls. These should take
    // and handle user interaction.
    return GetHTComponentForFrame(point, kBorderThickness, kBorderThickness,
                                  kResizeAreaCornerSize, kResizeAreaCornerSize,
                                  GetWidget()->widget_delegate()->CanResize());
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  views::Widget* widget_;

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
    return new OverlayWindowFrameView(widget);
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
      controls_background_view_(new views::View()),
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
  // on UI affordances, such as buttons.
  min_size_ = kMinWindowSize;

  // Initial size of the window is always 20% of the display width and height,
  // constrained by the min and max sizes. Only explicitly update this the first
  // time |current_size| is being calculated.
  // Once |current_size| is calculated at least once, it should stay within the
  // bounds of |min_size_| and |max_size_|.
  gfx::Size current_size;
  if (!current_bounds_.size().IsEmpty()) {
    current_size = current_bounds_.size();
  } else {
    current_size = gfx::Size(work_area.width() / 5, work_area.height() / 5);
    current_size.set_width(std::min(
        max_size_.width(), std::max(min_size_.width(), current_size.width())));
    current_size.set_height(
        std::min(max_size_.height(),
                 std::max(min_size_.height(), current_size.height())));
  }

  // Determine the window size by fitting |natural_size_| within
  // |current_size|, keeping to |natural_size_|'s aspect ratio.
  if (!natural_size_.IsEmpty())
    UpdateCurrentSizeWithAspectRatio(current_size);

  // The size is only empty the first time the window is shown. gfx::Point
  // cannot be checked for being unset as the default (0,0) is the valid
  // origin.
  if (!current_bounds_.size().IsEmpty()) {
    current_bounds_.set_size(current_size);
  } else {
    // The initial positioning is on the bottom right quadrant
    // of the primary display work area.
    int window_diff_width = work_area.width() - current_size.width();
    int window_diff_height = work_area.height() - current_size.height();

    // Keep a margin distance of 2% the average of the two window size
    // differences, keeping the margins consistent.
    int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;
    current_bounds_ = gfx::Rect(
        gfx::Point(window_diff_width - buffer, window_diff_height - buffer),
        current_size);
  }

  return current_bounds_;
}

void OverlayWindowViews::SetUpViews() {
  // Set up views::View that slightly darkens the video so the media controls
  // appear more prominently. This is especially important in cases with a
  // very light background.
  controls_background_view_->SetSize(GetBounds().size());
  controls_background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  GetControlsBackgroundLayer()->SetColor(SK_ColorBLACK);
  GetControlsBackgroundLayer()->SetOpacity(0.4f);

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
  play_pause_controls_view_->SetToggled(!controller_->IsPlayerActive());

  // Paint to ui::Layers to use in the OverlaySurfaceEmbedder.
  video_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  play_pause_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);

  // Don't show the controls until the mouse hovers over the window.
  GetControlsBackgroundLayer()->SetVisible(false);
  GetCloseControlsLayer()->SetVisible(false);
  GetPlayPauseControlsLayer()->SetVisible(false);
}

void OverlayWindowViews::UpdateCurrentSizeWithAspectRatio(gfx::Size new_size) {
  // This function will only be called once when this is true -- when the
  // window is initially created.
  if (current_bounds_.size().IsEmpty())
    return;

  // Check whether or not the new size and the video's natural size have the
  // same orientation (landscape vs. portrait). Otherwise, the usage of the
  // area checks below will flip the orientation of the video.
  bool is_natural_size_landscape =
      natural_size_.width() > natural_size_.height();
  bool is_new_size_landscape = new_size.width() > new_size.height();

  // TODO(apacible): Make resizing more strict. Currently, the window may
  // resize to not adhere to the aspect ratio while the bounds are being
  // dragged. When there is no more drag motion (e.g. mouse lifts), the window
  // snaps to adhere to the aspect ratio. Ideally, the window will always
  // adhere to the aspect ratio while in drag motion. http://crbug/829677.
  if (is_new_size_landscape == is_natural_size_landscape) {
    if (natural_size_.GetArea() > new_size.GetArea()) {
      current_bounds_.set_size(
          media::ScaleSizeToEncompassTarget(natural_size_, new_size));
    } else {
      current_bounds_.set_size(
          media::ScaleSizeToFitWithinTarget(natural_size_, new_size));
    }
  }
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

ui::Layer* OverlayWindowViews::GetControlsBackgroundLayer() {
  return controls_background_view_->layer();
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
      GetControlsBackgroundLayer()->SetVisible(true);
      GetCloseControlsLayer()->SetVisible(true);
      GetPlayPauseControlsLayer()->SetVisible(true);
      break;

    case ui::ET_MOUSE_EXITED:
      GetControlsBackgroundLayer()->SetVisible(false);
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
        // Retrieve expected active state based on what command was sent in
        // TogglePlayPause() since the IPC message may not have been propogated
        // the media player yet.
        bool is_active = controller_->TogglePlayPause();
        play_pause_controls_view_->SetToggled(is_active);
        event->SetHandled();
      }
      break;

    default:
      break;
  }
}

void OverlayWindowViews::OnNativeWidgetMove() {
  current_bounds_ = GetBounds();
}

void OverlayWindowViews::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  // Update the surface layer bounds to stretch / shrink the size of the shown
  // video in the window.
  if (controller_)
    controller_->UpdateLayerBounds();

  UpdateCurrentSizeWithAspectRatio(new_size);
  SetBounds(current_bounds_);
  views::Widget::OnNativeWidgetSizeChanged(current_bounds_.size());
}
