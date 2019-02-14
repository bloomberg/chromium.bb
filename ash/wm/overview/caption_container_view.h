// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_CAPTION_CONTAINER_VIEW_H_
#define ASH_WM_OVERVIEW_CAPTION_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class ButtonListener;
class ImageButton;
class ImageView;
class Label;
}  // namespace views

namespace ash {

class RoundedRectView;

// CaptionContainerView covers the overview window and listens for events. It
// also draws a header for overview mode which contains a icon, title and close
// button.
// TODO(sammiequon): Rename this to something which describes it better.
class ASH_EXPORT CaptionContainerView : public views::View {
 public:
  // The visibility of the header. It may be fully visible or invisible, or
  // everything but the close button is visible.
  enum class HeaderVisibility {
    kInvisible,
    kCloseButtonInvisibleOnly,
    kVisible,
  };

  CaptionContainerView(views::ButtonListener* listener, aura::Window* window);
  ~CaptionContainerView() override;

  // Returns |cannot_snap_container_|. This will create it if it has not been
  // already created.
  RoundedRectView* GetCannotSnapContainer();

  void SetHeaderVisibility(HeaderVisibility visibility);

  // Sets the visiblity of |backdrop_view_|. Creates it if it is null.
  void SetBackdropVisibility(bool visible);

  // Animates |cannot_snap_container_| to its visibility state.
  void SetCannotSnapLabelVisibility(bool visible);

  void ResetListener();

  // Set the title of the view, and also updates the accessiblity name.
  void SetTitle(const base::string16& title);

  views::View* GetListenerButton();
  views::ImageButton* GetCloseButton();

  views::View* header_view() { return header_view_; }
  views::Label* title_label() { return title_label_; }
  views::Label* cannot_snap_label() { return cannot_snap_label_; }
  RoundedRectView* backdrop_view() { return backdrop_view_; }

 protected:
  // views::View:
  void Layout() override;
  const char* GetClassName() const override;

 private:
  class OverviewCloseButton;
  class ShieldButton;

  // Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
  // otherwise. The tween type differs for |visible| and if |visible| is true
  // there is a slight delay before the animation begins. Does not animate if
  // opacity matches |visible|.
  void AnimateLayerOpacity(ui::Layer* layer, bool visible);

  // |listener_button_| handles input events and notifies the button listener.
  ShieldButton* listener_button_ = nullptr;

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
