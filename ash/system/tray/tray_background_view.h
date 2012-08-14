// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_

#include "ash/ash_export.h"
#include "ash/launcher/background_animator.h"
#include "ash/system/tray/tray_views.h"
#include "ash/wm/shelf_types.h"

namespace ash {
namespace internal {

class StatusAreaWidget;
class TrayBackground;

// Base class for children of StatusAreaWidget: SystemTray, WebNotificationTray.
// This class handles setting and animating the background when the Launcher
// his shown/hidden. It also inherits from ActionableView so that the tray
// items can override PerformAction when clicked on.

class ASH_EXPORT TrayBackgroundView : public internal::ActionableView,
                                      public BackgroundAnimatorDelegate {
 public:
  // Base class for tray containers. Sets the border and layout. The container
  // auto-resizes the widget when necessary.
  class TrayContainer : public views::View {
   public:
    explicit TrayContainer(ShelfAlignment alignment);
    virtual ~TrayContainer() {}

    void SetAlignment(ShelfAlignment alignment);

    void set_size(const gfx::Size& size) { size_ = size; }

    // Overridden from views::View.
    virtual gfx::Size GetPreferredSize() OVERRIDE;

   protected:
    // Overridden from views::View.
    virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;
    virtual void ChildVisibilityChanged(View* child) OVERRIDE;
    virtual void ViewHierarchyChanged(bool is_add,
                                      View* parent,
                                      View* child) OVERRIDE;

   private:
    void UpdateLayout();

    ShelfAlignment alignment_;
    gfx::Size size_;

    DISALLOW_COPY_AND_ASSIGN(TrayContainer);
  };

  explicit TrayBackgroundView(internal::StatusAreaWidget* status_area_widget);
  virtual ~TrayBackgroundView();

  // Overridden from views::View.
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // Overridden from internal::ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // Overridden from internal::BackgroundAnimatorDelegate.
  virtual void UpdateBackground(int alpha) OVERRIDE;

  // Called whenever the shelf alignment changes.
  virtual void SetShelfAlignment(ShelfAlignment alignment);

  // Sets |contents| as a child and sets its background to |background_|.
  void SetContents(views::View* contents);

  // Sets whether the tray paints a background. Default is true, but is set to
  // false if a window overlaps the shelf.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);

  // Called after all status area trays have been created. Sets the border
  // based on the position of the view.
  void SetBorder();

  StatusAreaWidget* status_area_widget() {
    return status_area_widget_;
  }
  TrayContainer* tray_container() const { return tray_container_; }
  ShelfAlignment shelf_alignment() const { return shelf_alignment_; }

 private:
  // Unowned pointer to parent widget.
  StatusAreaWidget* status_area_widget_;

  // Convenience pointer to the contents view.
  TrayContainer* tray_container_;

  // Shelf alignment.
  ShelfAlignment shelf_alignment_;

  // Owned by the view passed to SetContents().
  internal::TrayBackground* background_;

  internal::BackgroundAnimator hide_background_animator_;
  internal::BackgroundAnimator hover_background_animator_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
