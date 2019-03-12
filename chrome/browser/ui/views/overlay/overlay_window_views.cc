// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_image_button.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/mute_image_button.h"
#include "chrome/browser/ui/views/overlay/playback_image_button.h"
#include "chrome/browser/ui/views/overlay/resize_handle_button.h"
#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"
#include "chrome/browser/ui/views/overlay/track_image_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"
#include "ui/views/window/window_resize_utils.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "ui/aura/window.h"
#endif

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    content::PictureInPictureWindowController* controller) {
  return base::WrapUnique(new OverlayWindowViews(controller));
}

namespace {
constexpr gfx::Size kMinWindowSize = gfx::Size(144, 100);

const int kOverlayBorderThickness = 10;

// play/pause control scales both its width and height to be 30% the size of
// the smaller of the screen's width and height.
const float kPlayPauseControlRatioToWindow = 0.3;

// track controls scales both their width and height to be 60% the size of the
// play/pause control.
const float kTrackControlRatioToPlayPauseControl = 0.6;

const int kMinControlButtonSize = 48;
const int kControlButtonMargin = 12;

// Returns the quadrant the OverlayWindowViews is primarily in on the current
// work area.
OverlayWindowViews::WindowQuadrant GetCurrentWindowQuadrant(
    const gfx::Rect window_bounds,
    content::PictureInPictureWindowController* controller) {
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              controller->GetInitiatorWebContents()->GetTopLevelNativeWindow())
          .work_area();
  gfx::Point window_center = window_bounds.CenterPoint();

  // Check which quadrant the center of the window appears in.
  if (window_center.x() < work_area.width() / 2) {
    return (window_center.y() < work_area.height() / 2)
               ? OverlayWindowViews::WindowQuadrant::kTopLeft
               : OverlayWindowViews::WindowQuadrant::kBottomLeft;
  }
  return (window_center.y() < work_area.height() / 2)
             ? OverlayWindowViews::WindowQuadrant::kTopRight
             : OverlayWindowViews::WindowQuadrant::kBottomRight;
}

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

    constexpr int kResizeAreaCornerSize = 16;
    int window_component = GetHTComponentForFrame(
        point, kOverlayBorderThickness, kOverlayBorderThickness,
        kResizeAreaCornerSize, kResizeAreaCornerSize,
        GetWidget()->widget_delegate()->CanResize());

    // The media controls should take and handle user interaction.
    OverlayWindowViews* window = static_cast<OverlayWindowViews*>(widget_);
    if (window->AreControlsVisible() &&
        (window->GetBackToTabControlsBounds().Contains(point) ||
         window->GetMuteControlsBounds().Contains(point) ||
         window->GetSkipAdControlsBounds().Contains(point) ||
         window->GetCloseControlsBounds().Contains(point) ||
         window->GetPlayPauseControlsBounds().Contains(point) ||
         window->GetNextTrackControlsBounds().Contains(point) ||
         window->GetPreviousTrackControlsBounds().Contains(point))) {
      return window_component;
    }

#if defined(OS_CHROMEOS)
    // If the resize handle is clicked on, we want to force the hit test to
    // force a resize drag.
    if (window->AreControlsVisible() &&
        window->GetResizeHandleControlsBounds().Contains(point))
      return window->GetResizeHTComponent();
#endif

    // Allows for dragging and resizing the window.
    return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {}
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
      window_background_view_(new views::View()),
      video_view_(new views::View()),
      controls_scrim_view_(new views::View()),
      controls_parent_view_(new views::View()),
      back_to_tab_controls_view_(new views::BackToTabImageButton(this)),
      mute_controls_view_(new views::MuteImageButton(this)),
      skip_ad_controls_view_(new views::SkipAdLabelButton(this)),
      close_controls_view_(new views::CloseImageButton(this)),
#if defined(OS_CHROMEOS)
      resize_handle_view_(new views::ResizeHandleButton(this)),
#endif
      play_pause_controls_view_(new views::PlaybackImageButton(this)),
      next_track_controls_view_(new views::TrackImageButton(
          this,
          vector_icons::kMediaNextTrackIcon,
          l10n_util::GetStringUTF16(
              IDS_PICTURE_IN_PICTURE_NEXT_TRACK_CONTROL_ACCESSIBLE_TEXT))),
      previous_track_controls_view_(new views::TrackImageButton(
          this,
          vector_icons::kMediaPreviousTrackIcon,
          l10n_util::GetStringUTF16(
              IDS_PICTURE_IN_PICTURE_PREVIOUS_TRACK_CONTROL_ACCESSIBLE_TEXT))),
      hide_controls_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(2500 /* 2.5 seconds */),
          base::BindRepeating(&OverlayWindowViews::UpdateControlsVisibility,
                              base::Unretained(this),
                              false /* is_visible */)) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = CalculateAndUpdateWindowBounds();
  params.keep_on_top = true;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.name = "PictureInPictureWindow";
  params.layer_type = ui::LAYER_NOT_DRAWN;
#if defined(OS_CHROMEOS)
  // PIP windows are not activatable by default in ChromeOS. Although this can
  // be configured in ash/wm/window_state.cc, this is still meaningful when
  // window service is used, since the activatability isn't shared between
  // the window server and the client (here). crbug.com/923049 will happen
  // without this.
  // TODO(mukai): allow synchronizing activatability and remove this.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
#endif

  // Set WidgetDelegate for more control over |widget_|.
  params.delegate = new OverlayWindowWidgetDelegate(this);

  Init(params);
  SetUpViews();

#if defined(OS_CHROMEOS)
  GetNativeWindow()->SetProperty(ash::kWindowPipTypeKey, true);
#endif  // defined(OS_CHROMEOS)

  is_initialized_ = true;
}

OverlayWindowViews::~OverlayWindowViews() = default;

gfx::Rect OverlayWindowViews::CalculateAndUpdateWindowBounds() {
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(IsVisible()
                                        ? GetNativeWindow()
                                        : controller_->GetInitiatorWebContents()
                                              ->GetTopLevelNativeWindow())
          .work_area();

  // Upper bound size of the window is 50% of the display width and height.
  max_size_ = gfx::Size(work_area.width() / 2, work_area.height() / 2);

  // Lower bound size of the window is a fixed value to allow for minimal sizes
  // on UI affordances, such as buttons.
  min_size_ = kMinWindowSize;

  gfx::Size window_size = window_bounds_.size();
  if (!has_been_shown_) {
    window_size = gfx::Size(work_area.width() / 5, work_area.height() / 5);
    window_size.set_width(std::min(
        max_size_.width(), std::max(min_size_.width(), window_size.width())));
    window_size.set_height(
        std::min(max_size_.height(),
                 std::max(min_size_.height(), window_size.height())));
  }

  // Determine the window size by fitting |natural_size_| within
  // |window_size|, keeping to |natural_size_|'s aspect ratio.
  if (!window_size.IsEmpty() && !natural_size_.IsEmpty()) {
    float aspect_ratio = (float)natural_size_.width() / natural_size_.height();

    WindowQuadrant quadrant =
        GetCurrentWindowQuadrant(GetBounds(), controller_);
    views::HitTest hit_test;
    switch (quadrant) {
      case OverlayWindowViews::WindowQuadrant::kBottomRight:
        hit_test = views::HitTest::kTopLeft;
        break;
      case OverlayWindowViews::WindowQuadrant::kBottomLeft:
        hit_test = views::HitTest::kTopRight;
        break;
      case OverlayWindowViews::WindowQuadrant::kTopLeft:
        hit_test = views::HitTest::kBottomRight;
        break;
      case OverlayWindowViews::WindowQuadrant::kTopRight:
        hit_test = views::HitTest::kBottomLeft;
        break;
    }

    // Update the window size to adhere to the aspect ratio.
    gfx::Size min_size = min_size_;
    gfx::Size max_size = max_size_;
    views::WindowResizeUtils::SizeMinMaxToAspectRatio(aspect_ratio, &min_size,
                                                      &max_size);
    gfx::Rect window_rect(GetBounds().origin(), window_size);
    views::WindowResizeUtils::SizeRectToAspectRatio(
        hit_test, aspect_ratio, min_size, max_size, &window_rect);
    window_size.SetSize(window_rect.width(), window_rect.height());

    UpdateLayerBoundsWithLetterboxing(window_size);
  }

  // Use the previous window origin location, if exists.
  gfx::Point origin = window_bounds_.origin();

  int window_diff_width = work_area.right() - window_size.width();
  int window_diff_height = work_area.bottom() - window_size.height();

  // Keep a margin distance of 2% the average of the two window size
  // differences, keeping the margins consistent.
  int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;

  gfx::Point default_origin =
      gfx::Point(window_diff_width - buffer, window_diff_height - buffer);

  if (has_been_shown_) {
    // Make sure window is displayed entirely in the work area.
    origin.SetToMin(default_origin);
  } else {
    origin = default_origin;
  }

  window_bounds_ = gfx::Rect(origin, window_size);
  return window_bounds_;
}

void OverlayWindowViews::SetUpViews() {
  GetRootView()->SetPaintToLayer(ui::LAYER_TEXTURED);
  GetRootView()->layer()->set_name("RootView");
  GetRootView()->layer()->SetMasksToBounds(true);

  // views::View that is displayed when video is hidden. ----------------------
  // Adding an extra pixel to width/height makes sure controls background cover
  // entirely window when platform has fractional scale applied.
  gfx::Rect larger_window_bounds = GetBounds();
  larger_window_bounds.Inset(-1, -1);
  window_background_view_->SetSize(larger_window_bounds.size());
  window_background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  window_background_view_->layer()->set_name("WindowBackgroundView");
  GetWindowBackgroundLayer()->SetColor(SK_ColorBLACK);

  // views::View that holds the scrim, which appears with the controls. -------
  controls_scrim_view_->SetSize(GetBounds().size());
  controls_scrim_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  controls_scrim_view_->layer()->set_name("ControlsScrimView");
  GetControlsScrimLayer()->SetColor(gfx::kGoogleGrey900);
  GetControlsScrimLayer()->SetOpacity(0.43f);

  // view::View that holds the controls. --------------------------------------
  controls_parent_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  controls_parent_view_->SetSize(GetBounds().size());
  controls_parent_view_->layer()->SetFillsBoundsOpaquely(false);
  controls_parent_view_->layer()->set_name("ControlsParentView");
  controls_parent_view_->set_owned_by_client();

  // views::View that closes the window and focuses initiator tab. ------------
  back_to_tab_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  back_to_tab_controls_view_->layer()->SetFillsBoundsOpaquely(false);
  back_to_tab_controls_view_->layer()->set_name("BackToTabControlsView");
  back_to_tab_controls_view_->set_owned_by_client();

  // views::View that holds the mute image button. -------------------------
  mute_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  mute_controls_view_->layer()->SetFillsBoundsOpaquely(false);
  mute_controls_view_->layer()->set_name("MuteControlsView");
  mute_controls_view_->SetMutedState(kNoAudio);
  mute_controls_view_->set_owned_by_client();

  // views::View that holds the skip-ad label button. -------------------------
  skip_ad_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  skip_ad_controls_view_->layer()->SetFillsBoundsOpaquely(true);
  skip_ad_controls_view_->layer()->set_name("SkipAdControlsView");
  skip_ad_controls_view_->set_owned_by_client();

  // views::View that closes the window. --------------------------------------
  close_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view_->layer()->SetFillsBoundsOpaquely(false);
  close_controls_view_->layer()->set_name("CloseControlsView");
  close_controls_view_->set_owned_by_client();

  // view::View that holds the video. -----------------------------------------
  video_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  video_view_->layer()->set_name("VideoView");

  // views::View that toggles play/pause/replay. ------------------------------
  play_pause_controls_view_->SetPlaybackState(
      controller_->IsPlayerActive() ? kPlaying : kPaused);
  play_pause_controls_view_->set_owned_by_client();

  // views::View that holds the next-track image button. ----------------------
  next_track_controls_view_->set_owned_by_client();

  // views::View that holds the previous-track image button. ------------------
  previous_track_controls_view_->set_owned_by_client();

#if defined(OS_CHROMEOS)
  // views::View that shows the affordance that the window can be resized. ----
  resize_handle_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  resize_handle_view_->layer()->SetFillsBoundsOpaquely(false);
  resize_handle_view_->layer()->set_name("ResizeHandleView");
  resize_handle_view_->set_owned_by_client();
#endif

  // Set up view::Views hierarchy. --------------------------------------------
  controls_parent_view_->AddChildView(play_pause_controls_view_.get());
  controls_parent_view_->AddChildView(next_track_controls_view_.get());
  controls_parent_view_->AddChildView(previous_track_controls_view_.get());
  GetContentsView()->AddChildView(controls_scrim_view_.get());
  GetContentsView()->AddChildView(controls_parent_view_.get());
  GetContentsView()->AddChildView(skip_ad_controls_view_.get());
  GetContentsView()->AddChildView(back_to_tab_controls_view_.get());
  GetContentsView()->AddChildView(mute_controls_view_.get());
  GetContentsView()->AddChildView(close_controls_view_.get());
#if defined(OS_CHROMEOS)
  GetContentsView()->AddChildView(resize_handle_view_.get());
#endif

  UpdateButtonControlsSize();
  UpdateControlsVisibility(false);
}

void OverlayWindowViews::UpdateLayerBoundsWithLetterboxing(
    gfx::Size window_size) {
  // This is the case when the window is initially created or the video surface
  // id has not been embedded.
  if (window_bounds_.size().IsEmpty() || natural_size_.IsEmpty())
    return;

  gfx::Rect letterbox_region = media::ComputeLetterboxRegion(
      gfx::Rect(gfx::Point(0, 0), window_size), natural_size_);
  if (letterbox_region.IsEmpty())
    return;

  // To avoid one-pixel black line in the window when floated aspect ratio is
  // not perfect (e.g. 848x480 for 16:9 video), letterbox region size is the
  // same as window size.
  if ((std::abs(window_size.width() - letterbox_region.width()) <= 1) &&
      (std::abs(window_size.height() - letterbox_region.height()) <= 1)) {
    letterbox_region.set_size(window_size);
  }

  gfx::Size letterbox_size = letterbox_region.size();
  gfx::Point origin =
      gfx::Point((window_size.width() - letterbox_size.width()) / 2,
                 (window_size.height() - letterbox_size.height()) / 2);

  video_bounds_.set_origin(origin);
  video_bounds_.set_size(letterbox_region.size());

  // Update the layout of the controls.
  UpdateControlsBounds();

  // Update the surface layer bounds to scale with window size changes.
  controller_->UpdateLayerBounds();
}

void OverlayWindowViews::UpdateControlsVisibility(bool is_visible) {
  play_pause_controls_view_->SetVisible(is_visible &&
                                        !always_hide_play_pause_button_);
  next_track_controls_view_->ToggleVisibility(is_visible &&
                                              show_next_track_button_);
  previous_track_controls_view_->ToggleVisibility(is_visible &&
                                                  show_previous_track_button_);
  GetControlsScrimLayer()->SetVisible(is_visible);
  GetControlsParentLayer()->SetVisible(is_visible);
  GetBackToTabControlsLayer()->SetVisible(is_visible);
  GetCloseControlsLayer()->SetVisible(is_visible);

  // We need to do more than usual visibility change because otherwise control
  // is accessible via accessibility tools.
  mute_controls_view_->ToggleVisibility(is_visible);
  skip_ad_controls_view_->ToggleVisibility(is_visible && show_skip_ad_button_);

#if defined(OS_CHROMEOS)
  GetResizeHandleLayer()->SetVisible(is_visible);
#endif
}

void OverlayWindowViews::UpdateControlsBounds() {
  // Adding an extra pixel to width/height makes sure the scrim covers the
  // entire window when the platform has fractional scaling applied.
  gfx::Rect larger_window_bounds = GetBounds();
  larger_window_bounds.Inset(-1, -1);
  controls_scrim_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), larger_window_bounds.size()));

  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  back_to_tab_controls_view_->SetPosition(GetBounds().size(), quadrant);
  mute_controls_view_->SetPosition(GetBounds().size(), quadrant);
  skip_ad_controls_view_->SetPosition(GetBounds().size());
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);
#if defined(OS_CHROMEOS)
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
#endif

  controls_parent_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), GetBounds().size()));

  int mid_window_x = GetBounds().size().width() / 2;
  play_pause_controls_view_->SetBoundsRect(CalculateControlsBounds(
      mid_window_x - play_pause_controls_view_->size().width() / 2,
      play_pause_controls_view_->size()));

  if (show_next_track_button_) {
    next_track_controls_view_->SetBoundsRect(CalculateControlsBounds(
        mid_window_x + play_pause_controls_view_->size().width() / 2 +
            kControlButtonMargin,
        next_track_controls_view_->GetLastVisibleSize()));
  }
  if (show_previous_track_button_) {
    previous_track_controls_view_->SetBoundsRect(CalculateControlsBounds(
        mid_window_x - play_pause_controls_view_->size().width() / 2 -
            previous_track_controls_view_->GetLastVisibleSize().width() -
            kControlButtonMargin,
        previous_track_controls_view_->GetLastVisibleSize()));
  }
}

gfx::Rect OverlayWindowViews::CalculateControlsBounds(int x,
                                                      const gfx::Size& size) {
  return gfx::Rect(
      gfx::Point(x, (GetBounds().size().height() - size.height()) / 2), size);
}

void OverlayWindowViews::UpdateButtonControlsSize() {
  const gfx::Size window_size = GetBounds().size();
  int scaled_button_dimension =
      window_size.width() < window_size.height()
          ? window_size.width() * kPlayPauseControlRatioToWindow
          : window_size.height() * kPlayPauseControlRatioToWindow;

  int new_button_dimension =
      std::max(kMinControlButtonSize, scaled_button_dimension);

  const gfx::Size play_pause_button_size(new_button_dimension,
                                         new_button_dimension);

  play_pause_controls_view_->SetSize(play_pause_button_size);
  gfx::Size track_button_size = gfx::ScaleToRoundedSize(
      play_pause_button_size, kTrackControlRatioToPlayPauseControl);
  next_track_controls_view_->SetSize(track_button_size);
  previous_track_controls_view_->SetSize(track_button_size);
}

bool OverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void OverlayWindowViews::Close() {
  views::Widget::Close();
}

void OverlayWindowViews::ShowInactive() {
  views::Widget::ShowInactive();
#if defined(OS_CHROMEOS)
  // For rounded corners.
  if (ash::features::IsPipRoundedCornersEnabled()) {
    decorator_ = std::make_unique<ash::RoundedCornerDecorator>(
        GetNativeWindow(), GetNativeWindow(), GetRootView()->layer(),
        ash::kPipRoundedCornerRadius);
  }
#endif

  // If this is not the first time the window is shown, this will be a no-op.
  has_been_shown_ = true;
}

void OverlayWindowViews::Hide() {
  views::Widget::Hide();
}

bool OverlayWindowViews::IsVisible() const {
  return is_initialized_ ? views::Widget::IsVisible() : false;
}

bool OverlayWindowViews::IsAlwaysOnTop() const {
  return true;
}

ui::Layer* OverlayWindowViews::GetLayer() {
  return GetRootView()->layer();
}

gfx::Rect OverlayWindowViews::GetBounds() const {
  return views::Widget::GetRestoredBounds();
}

void OverlayWindowViews::UpdateVideoSize(const gfx::Size& natural_size) {
  DCHECK(!natural_size.IsEmpty());
  natural_size_ = natural_size;
  SetAspectRatio(gfx::SizeF(natural_size_));

  // Update the views::Widget bounds to adhere to sizing spec. This will also
  // update the layout of the controls.
  SetBounds(CalculateAndUpdateWindowBounds());
}

void OverlayWindowViews::SetPlaybackState(PlaybackState playback_state) {
  playback_state_for_testing_ = playback_state;
  play_pause_controls_view_->SetPlaybackState(playback_state);
}

void OverlayWindowViews::SetAlwaysHidePlayPauseButton(bool is_visible) {
  always_hide_play_pause_button_ = !is_visible;
}

void OverlayWindowViews::SetMutedState(MutedState muted_state) {
  muted_state_for_testing_ = muted_state;
  mute_controls_view_->SetMutedState(muted_state);
}

void OverlayWindowViews::SetSkipAdButtonVisibility(bool is_visible) {
  show_skip_ad_button_ = is_visible;
}

void OverlayWindowViews::SetNextTrackButtonVisibility(bool is_visible) {
  if (show_next_track_button_ == is_visible)
    return;

  show_next_track_button_ = is_visible;
  UpdateButtonControlsSize();
  UpdateControlsBounds();
}

void OverlayWindowViews::SetPreviousTrackButtonVisibility(bool is_visible) {
  if (show_previous_track_button_ == is_visible)
    return;

  show_previous_track_button_ = is_visible;
  UpdateButtonControlsSize();
  UpdateControlsBounds();
}

ui::Layer* OverlayWindowViews::GetWindowBackgroundLayer() {
  return window_background_view_->layer();
}

ui::Layer* OverlayWindowViews::GetVideoLayer() {
  return video_view_->layer();
}

gfx::Rect OverlayWindowViews::GetVideoBounds() {
  return video_bounds_;
}

void OverlayWindowViews::OnNativeBlur() {
  // Controls should be hidden when there is no more focus on the window. This
  // is used for tabbing and touch interactions. For mouse interactions, the
  // window cannot be blurred before the ui::ET_MOUSE_EXITED event is handled.
  if (is_initialized_)
    UpdateControlsVisibility(false);

  views::Widget::OnNativeBlur();
}

void OverlayWindowViews::OnNativeWidgetDestroyed() {
  controller_->OnWindowDestroyed();
}

gfx::Size OverlayWindowViews::GetMinimumSize() const {
  return min_size_;
}

gfx::Size OverlayWindowViews::GetMaximumSize() const {
  return max_size_;
}

void OverlayWindowViews::OnNativeWidgetMove() {
  // Hide the controls when the window is moving. The controls will reappear
  // when the user interacts with the window again.
  if (is_initialized_)
    UpdateControlsVisibility(false);

  // Update the existing |window_bounds_| when the window moves. This allows
  // the window to reappear with the same origin point when a new video is
  // shown.
  window_bounds_ = GetBounds();

#if defined(OS_CHROMEOS)
  // Update the positioning of some icons when the window is moved.
  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  back_to_tab_controls_view_->SetPosition(GetBounds().size(), quadrant);
  mute_controls_view_->SetPosition(GetBounds().size(), quadrant);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
#endif
}

void OverlayWindowViews::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  // Hide the controls when the window is being resized. The controls will
  // reappear when the user interacts with the window again.
  if (is_initialized_)
    UpdateControlsVisibility(false);

  // Update the view layers to scale to |new_size|.
  UpdateButtonControlsSize();
  UpdateLayerBoundsWithLetterboxing(new_size);

  views::Widget::OnNativeWidgetSizeChanged(new_size);
}

void OverlayWindowViews::OnNativeWidgetWorkspaceChanged() {
  // TODO(apacible): Update sizes and maybe resize the current
  // Picture-in-Picture window. Currently, switching between workspaces on linux
  // does not trigger this function. http://crbug.com/819673
}

void OverlayWindowViews::OnKeyEvent(ui::KeyEvent* event) {
  // Every time a user uses a keyboard to interact on the window, restart the
  // timer to automatically hide the controls.
  hide_controls_timer_.Reset();

  // Any keystroke will make the controls visible, if not already. The Tab key
  // needs to be handled separately.
  // If the controls are already visible, this is a no-op.
  if (event->type() == ui::ET_KEY_PRESSED ||
      event->key_code() == ui::VKEY_TAB) {
    UpdateControlsVisibility(true);
  }

// On Mac, the space key event isn't automatically handled. Only handle space
// for TogglePlayPause() since tabbing between the buttons is not supported and
// there is no focus affordance on the buttons.
#if defined(OS_MACOSX)
  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_SPACE) {
    TogglePlayPause();
    event->SetHandled();
  }
#endif  // OS_MACOSX

// On Windows, the Alt+F4 keyboard combination closes the window. Only handle
// closure on key press so Close() is not called a second time when the key
// is released.
#if defined(OS_WIN)
  if (event->type() == ui::ET_KEY_PRESSED && event->IsAltDown() &&
      event->key_code() == ui::VKEY_F4) {
    controller_->Close(true /* should_pause_video */,
                       true /* should_reset_pip_player */);
    event->SetHandled();
  }
#endif  // OS_WIN

  views::Widget::OnKeyEvent(event);
}

void OverlayWindowViews::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
// Only show the media controls when the mouse is hovering over the window.
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
      UpdateControlsVisibility(true);
      break;

    case ui::ET_MOUSE_EXITED:
      // On Windows, ui::ET_MOUSE_EXITED is triggered when hovering over the
      // media controls because of the HitTest. This check ensures the controls
      // are visible if the mouse is still over the window.
      if (!GetVideoBounds().Contains(event->location()))
        UpdateControlsVisibility(false);
      break;

    default:
      break;
  }

  // If the user interacts with the window using a mouse, stop the timer to
  // automatically hide the controls.
  hide_controls_timer_.Reset();

  views::Widget::OnMouseEvent(event);
}

void OverlayWindowViews::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;

  // Every time a user taps on the window, restart the timer to automatically
  // hide the controls.
  hide_controls_timer_.Reset();

  // If the controls were not shown, make them visible. All controls related
  // layers are expected to have the same visibility.
  // TODO(apacible): This placeholder logic should be updated with touchscreen
  // specific investigation. https://crbug/854373
  if (!AreControlsVisible()) {
    UpdateControlsVisibility(true);
    return;
  }

  if (GetBackToTabControlsBounds().Contains(event->location())) {
    controller_->CloseAndFocusInitiator();
    RecordTapGesture(OverlayWindowControl::kBackToTab);
    event->SetHandled();
  } else if (GetMuteControlsBounds().Contains(event->location())) {
    ToggleMute();
    RecordTapGesture(OverlayWindowControl::kMute);
    event->SetHandled();
  } else if (GetSkipAdControlsBounds().Contains(event->location())) {
    controller_->SkipAd();
    RecordTapGesture(OverlayWindowControl::kSkipAd);
    event->SetHandled();
  } else if (GetCloseControlsBounds().Contains(event->location())) {
    controller_->Close(true /* should_pause_video */,
                       true /* should_reset_pip_player */);
    RecordTapGesture(OverlayWindowControl::kClose);
    event->SetHandled();
  } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
    TogglePlayPause();
    RecordTapGesture(OverlayWindowControl::kPlayPause);
    event->SetHandled();
  } else if (GetNextTrackControlsBounds().Contains(event->location())) {
    controller_->NextTrack();
    RecordTapGesture(OverlayWindowControl::kNextTrack);
    event->SetHandled();
  } else if (GetPreviousTrackControlsBounds().Contains(event->location())) {
    controller_->PreviousTrack();
    RecordTapGesture(OverlayWindowControl::kPreviousTrack);
    event->SetHandled();
  }
}

void OverlayWindowViews::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == back_to_tab_controls_view_.get()) {
    controller_->CloseAndFocusInitiator();
    RecordButtonPressed(OverlayWindowControl::kBackToTab);
  }

  if (sender == mute_controls_view_.get()) {
    ToggleMute();
    RecordButtonPressed(OverlayWindowControl::kMute);
  }

  if (sender == skip_ad_controls_view_.get()) {
    controller_->SkipAd();
    RecordButtonPressed(OverlayWindowControl::kSkipAd);
  }

  if (sender == close_controls_view_.get()) {
    controller_->Close(true /* should_pause_video */,
                       true /* should_reset_pip_player */);
    RecordButtonPressed(OverlayWindowControl::kClose);
  }

  if (sender == play_pause_controls_view_.get()) {
    TogglePlayPause();
    RecordButtonPressed(OverlayWindowControl::kPlayPause);
  }

  if (sender == next_track_controls_view_.get()) {
    controller_->NextTrack();
    RecordButtonPressed(OverlayWindowControl::kNextTrack);
  }

  if (sender == previous_track_controls_view_.get()) {
    controller_->PreviousTrack();
    RecordButtonPressed(OverlayWindowControl::kPreviousTrack);
  }
}

void OverlayWindowViews::RecordTapGesture(OverlayWindowControl window_control) {
  UMA_HISTOGRAM_ENUMERATION("PictureInPictureWindow.TapGesture",
                            window_control);
}

void OverlayWindowViews::RecordButtonPressed(
    OverlayWindowControl window_control) {
  UMA_HISTOGRAM_ENUMERATION("PictureInPictureWindow.ButtonPressed",
                            window_control);
}

gfx::Rect OverlayWindowViews::GetBackToTabControlsBounds() {
  return back_to_tab_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetMuteControlsBounds() {
  return mute_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetSkipAdControlsBounds() {
  return skip_ad_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetCloseControlsBounds() {
  return close_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetResizeHandleControlsBounds() {
  return resize_handle_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetPlayPauseControlsBounds() {
  return play_pause_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetNextTrackControlsBounds() {
  return next_track_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetPreviousTrackControlsBounds() {
  return previous_track_controls_view_->GetMirroredBounds();
}

int OverlayWindowViews::GetResizeHTComponent() const {
  return resize_handle_view_->GetHTComponent();
}

bool OverlayWindowViews::AreControlsVisible() const {
  return controls_scrim_view_->layer()->visible();
}

ui::Layer* OverlayWindowViews::GetControlsScrimLayer() {
  return controls_scrim_view_->layer();
}

ui::Layer* OverlayWindowViews::GetBackToTabControlsLayer() {
  return back_to_tab_controls_view_->layer();
}

ui::Layer* OverlayWindowViews::GetMuteControlsLayer() {
  return mute_controls_view_->layer();
}

ui::Layer* OverlayWindowViews::GetCloseControlsLayer() {
  return close_controls_view_->layer();
}

ui::Layer* OverlayWindowViews::GetResizeHandleLayer() {
  return resize_handle_view_->layer();
}

ui::Layer* OverlayWindowViews::GetControlsParentLayer() {
  return controls_parent_view_->layer();
}

void OverlayWindowViews::TogglePlayPause() {
  // Retrieve expected active state based on what command was sent in
  // TogglePlayPause() since the IPC message may not have been propagated
  // the media player yet.
  bool is_active = controller_->TogglePlayPause();
  play_pause_controls_view_->SetPlaybackState(is_active ? kPlaying : kPaused);
}

void OverlayWindowViews::ToggleMute() {
  // Retrieve expected active state based on what command was sent in
  // ToggleMute() since the IPC message may not have been propagated
  // the media player yet.
  bool muted = controller_->ToggleMute();
  mute_controls_view_->SetMutedState(muted ? kMuted : kUnmuted);
}

views::PlaybackImageButton*
OverlayWindowViews::play_pause_controls_view_for_testing() const {
  return play_pause_controls_view_.get();
}

views::TrackImageButton*
OverlayWindowViews::next_track_controls_view_for_testing() const {
  return next_track_controls_view_.get();
}

views::TrackImageButton*
OverlayWindowViews::previous_track_controls_view_for_testing() const {
  return previous_track_controls_view_.get();
}

views::SkipAdLabelButton*
OverlayWindowViews::skip_ad_controls_view_for_testing() const {
  return skip_ad_controls_view_.get();
}

gfx::Point OverlayWindowViews::back_to_tab_image_position_for_testing() const {
  return back_to_tab_controls_view_->origin();
}

gfx::Point OverlayWindowViews::close_image_position_for_testing() const {
  return close_controls_view_->origin();
}

gfx::Point OverlayWindowViews::mute_image_position_for_testing() const {
  return mute_controls_view_->origin();
}

gfx::Point OverlayWindowViews::resize_handle_position_for_testing() const {
  return resize_handle_view_->origin();
}

views::View* OverlayWindowViews::controls_parent_view_for_testing() const {
  return controls_parent_view_.get();
}

OverlayWindowViews::PlaybackState
OverlayWindowViews::playback_state_for_testing() const {
  return playback_state_for_testing_;
}

OverlayWindowViews::MutedState OverlayWindowViews::muted_state_for_testing()
    const {
  return muted_state_for_testing_;
}
