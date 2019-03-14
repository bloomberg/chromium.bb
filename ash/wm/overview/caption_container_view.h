// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_CAPTION_CONTAINER_VIEW_H_
#define ASH_WM_OVERVIEW_CAPTION_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class ImageButton;
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {

class RoundedRectView;

// CaptionContainerView covers the overview window and listens for events. It
// also draws a header for overview mode which contains a icon, title and close
// button.
// TODO(sammiequon): Rename this to something which describes it better.
class ASH_EXPORT CaptionContainerView : public views::Button {
 public:
  // The visibility of the header. It may be fully visible or invisible, or
  // everything but the close button is visible.
  enum class HeaderVisibility {
    kInvisible,
    kCloseButtonInvisibleOnly,
    kVisible,
  };

  class EventDelegate {
   public:
    // TODO: Maybe consolidate into just mouse and gesture events.
    virtual void HandlePressEvent(const gfx::PointF& location_in_screen) = 0;
    virtual void HandleDragEvent(const gfx::PointF& location_in_screen) = 0;
    virtual void HandleReleaseEvent(const gfx::PointF& location_in_screen) = 0;
    virtual void HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                                       float velocity_x,
                                       float velocity_y) = 0;
    virtual void HandleLongPressEvent(
        const gfx::PointF& location_in_screen) = 0;
    virtual void HandleTapEvent() = 0;
    virtual void HandleGestureEndEvent() = 0;
    virtual void HandleCloseButtonClicked() = 0;
    virtual bool ShouldIgnoreGestureEvents() = 0;

   protected:
    virtual ~EventDelegate() {}
  };

  CaptionContainerView(EventDelegate* event_delegate, aura::Window* window);
  ~CaptionContainerView() override;

  // Returns |cannot_snap_container_|. This will create it if it has not been
  // already created.
  RoundedRectView* GetCannotSnapContainer();

  void SetHeaderVisibility(HeaderVisibility visibility);

  // Sets the visiblity of |backdrop_view_|. Creates it if it is null.
  void SetBackdropVisibility(bool visible);

  // Animates |cannot_snap_container_| to its visibility state.
  void SetCannotSnapLabelVisibility(bool visible);

  void ResetEventDelegate();

  // Set the title of the view, and also updates the accessiblity name.
  void SetTitle(const base::string16& title);

  views::ImageButton* GetCloseButton();

  views::View* header_view() { return header_view_; }
  views::Label* title_label() { return title_label_; }
  views::Label* cannot_snap_label() { return cannot_snap_label_; }
  RoundedRectView* backdrop_view() { return backdrop_view_; }

 protected:
  // views::View:
  void Layout() override;
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool CanAcceptEvent(const ui::Event& event) override;

 private:
  class OverviewCloseButton;

  // Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
  // otherwise. The tween type differs for |visible| and if |visible| is true
  // there is a slight delay before the animation begins. Does not animate if
  // opacity matches |visible|.
  void AnimateLayerOpacity(ui::Layer* layer, bool visible);

  // The delegate which all the events get forwarded to.
  EventDelegate* event_delegate_;

  // View which contains the icon, title and close button.
  views::View* header_view_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::ImageView* image_view_ = nullptr;
  OverviewCloseButton* close_button_ = nullptr;

  // A text label in the center of the window warning users that
  // this window cannot be snapped for splitview.
  views::Label* cannot_snap_label_ = nullptr;
  // Use |cannot_snap_container_| to specify the padding surrounding
  // |cannot_snap_label_| and to give the label rounded corners.
  RoundedRectView* cannot_snap_container_ = nullptr;

  // A view that covers the area except the header. It is null when the window
  // associated is not pillar or letter boxed.
  RoundedRectView* backdrop_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CaptionContainerView);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_CAPTION_CONTAINER_VIEW_H_
