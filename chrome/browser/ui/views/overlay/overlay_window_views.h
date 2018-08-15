// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_

#include "content/public/browser/overlay_window.h"

#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

namespace views {
class ControlImageButton;
class CloseImageButton;
class ToggleImageButton;
}  // namespace views

// The Chrome desktop implementation of OverlayWindow. This will only be
// implemented in views, which will support all desktop platforms.
class OverlayWindowViews : public content::OverlayWindow,
                           public views::ButtonListener,
                           public views::Widget {
 public:
  explicit OverlayWindowViews(
      content::PictureInPictureWindowController* controller);
  ~OverlayWindowViews() override;

  // OverlayWindow:
  bool IsActive() const override;
  void Close() override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  bool IsAlwaysOnTop() const override;
  ui::Layer* GetLayer() override;
  gfx::Rect GetBounds() const override;
  void UpdateVideoSize(const gfx::Size& natural_size) override;
  void SetPlaybackState(PlaybackState playback_state) override;
  void SetPictureInPictureCustomControls(
      const std::vector<blink::PictureInPictureControlInfo>& controls) override;
  ui::Layer* GetWindowBackgroundLayer() override;
  ui::Layer* GetVideoLayer() override;
  gfx::Rect GetVideoBounds() override;

  // views::Widget:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::internal::NativeWidgetDelegate:
  void OnNativeFocus() override;
  void OnNativeBlur() override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;
  void OnNativeWidgetDestroyed() override;

  // Gets the bounds of the controls.
  gfx::Rect GetCloseControlsBounds();
  gfx::Rect GetPlayPauseControlsBounds();
  gfx::Rect GetFirstCustomControlsBounds();
  gfx::Rect GetSecondCustomControlsBounds();

  // Send the message that a custom control on |this| has been clicked.
  void ClickCustomControl(const std::string& control_id);

  views::ToggleImageButton* play_pause_controls_view_for_testing() const;
  views::View* controls_parent_view_for_testing() const;

 private:
  // Possible positions for the custom controls added to the window.
  enum class ControlPosition { kLeft, kRight };

  // Determine the intended bounds of |this|. This should be called when there
  // is reason for the bounds to change, such as switching primary displays or
  // playing a new video (i.e. different aspect ratio). This also updates
  // |min_size_| and |max_size_|.
  gfx::Rect CalculateAndUpdateWindowBounds();

  // Set up the views::Views that will be shown on the window.
  void SetUpViews();

  // Update the bounds of the layers on the window. This may introduce
  // letterboxing.
  void UpdateLayerBoundsWithLetterboxing(gfx::Size window_size);

  // Updates the controls view::Views to reflect |is_visible|.
  void UpdateControlsVisibility(bool is_visible);

  // Updates the bounds of the controls.
  void UpdateControlsBounds();

  // Update the size the controls views use as the size of the window changes.
  void UpdateButtonSize();

  // Update the size of each controls view as the size of the window changes.
  void UpdateCustomControlsSize(views::ControlImageButton*);
  void UpdatePlayPauseControlsSize();

  void SetUpCustomControl(std::unique_ptr<views::ControlImageButton>&,
                          const blink::PictureInPictureControlInfo&,
                          ControlPosition);

  // Returns whether there is exactly one custom control on the window.
  bool OnlyOneCustomControlAdded();

  // Calculate and set the bounds of the controls.
  gfx::Rect CalculateControlsBounds(int x, const gfx::Size& size);
  void UpdateControlsPositions();

  // Sets the bounds of the custom controls.
  void SetFirstCustomControlsBounds();
  void SetSecondCustomControlsBounds();

  ui::Layer* GetControlsBackgroundLayer();
  ui::Layer* GetControlsParentLayer();

  // Toggles the play/pause control through the |controller_| and updates the
  // |play_pause_controls_view_| toggled state to reflect the current playing
  // state.
  void TogglePlayPause();

  // Not owned; |controller_| owns |this|.
  content::PictureInPictureWindowController* controller_;

  // Whether or not the components of the window has been set up. This is used
  // as a check as some event handlers (e.g. focus) is propogated to the window
  // before its contents is initialized. This is only set once.
  bool is_initialized_ = false;

  // Whether or not the controls of the window should be shown. This is used in
  // some event handlers (e.g. focus).
  bool should_show_controls_ = false;

  // Whether or not the window has been shown before. This is used to determine
  // sizing and placement. This is different from checking whether the window
  // components has been initialized.
  bool has_been_shown_ = false;

  // The upper and lower bounds of |current_size_|. These are determined by the
  // size of the primary display work area when Picture-in-Picture is initiated.
  // TODO(apacible): Update these bounds when the display the window is on
  // changes. http://crbug.com/819673
  gfx::Size min_size_;
  gfx::Size max_size_;

  // Current sizes of the control views on the Picture-in-Picture window.
  gfx::Size button_size_;

  // Current bounds of the Picture-in-Picture window.
  gfx::Rect window_bounds_;

  // Bounds of |video_view_|.
  gfx::Rect video_bounds_;

  // The natural size of the video to show. This is used to compute sizing and
  // ensuring factors such as aspect ratio is maintained.
  gfx::Size natural_size_;

  // Views to be shown.
  std::unique_ptr<views::View> window_background_view_;
  std::unique_ptr<views::View> video_view_;
  // |controls_background_view_| will dim the screen when the mouse hovers over.
  // |controls_parent_view_| will hold all controls and manage them except for
  // |close_controls_view_|
  std::unique_ptr<views::View> controls_background_view_;
  std::unique_ptr<views::View> controls_parent_view_;
  std::unique_ptr<views::CloseImageButton> close_controls_view_;
  std::unique_ptr<views::ToggleImageButton> play_pause_controls_view_;
  std::unique_ptr<views::ControlImageButton> first_custom_controls_view_;
  std::unique_ptr<views::ControlImageButton> second_custom_controls_view_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
