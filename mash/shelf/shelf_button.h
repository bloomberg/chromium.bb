// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_BUTTON_H_
#define MASH_SHELF_SHELF_BUTTON_H_

#include "base/macros.h"
#include "mash/shelf/shelf_types.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"

namespace mash {
namespace shelf {
class ShelfButtonHost;

// Button used for items on the launcher, except for the AppList.
class ShelfButton : public views::CustomButton {
 public:
  static const char kViewClassName[];

  // Used to indicate the current state of the button.
  enum State {
    // Nothing special. Usually represents an app shortcut item with no running
    // instance.
    STATE_NORMAL    = 0,
    // Button has mouse hovering on it.
    STATE_HOVERED   = 1 << 0,
    // Underlying ShelfItem has a running instance.
    STATE_RUNNING   = 1 << 1,
    // Underlying ShelfItem is active (i.e. has focus).
    STATE_ACTIVE    = 1 << 2,
    // Underlying ShelfItem needs user's attention.
    STATE_ATTENTION = 1 << 3,
    STATE_FOCUSED   = 1 << 4,
    // Hide the status (temporarily for some animations).
    STATE_HIDDEN = 1 << 5,
  };

  ~ShelfButton() override;

  // Called to create an instance of a ShelfButton.
  static ShelfButton* Create(views::ButtonListener* listener,
                             ShelfButtonHost* host);

  // Sets the image to display for this entry.
  void SetImage(const gfx::ImageSkia& image);

  // Retrieve the image to show proxy operations.
  const gfx::ImageSkia& GetImage() const;

  // |state| is or'd into the current state.
  void AddState(State state);
  void ClearState(State state);
  int state() const { return state_; }

  // Returns the bounds of the icon.
  gfx::Rect GetIconBounds() const;

  // Overrides to views::CustomButton:
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;

  // View override - needed by unit test.
  void OnMouseCaptureLost() override;

 protected:
  ShelfButton(views::ButtonListener* listener,
              ShelfButtonHost* host);

  // Class that draws the icon part of a button, so it can be animated
  // independently of the rest. This can be subclassed to provide a custom
  // implementation, by overriding CreateIconView().
  class IconView : public views::ImageView {
   public:
    IconView();
    ~IconView() override;

    void set_icon_size(int icon_size) { icon_size_ = icon_size; }
    int icon_size() const { return icon_size_; }

   private:
    // Set to non-zero to force icons to be resized to fit within a square,
    // while maintaining original proportions.
    int icon_size_;

    DISALLOW_COPY_AND_ASSIGN(IconView);
  };

  // View overrides:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Sets the icon image with a shadow.
  void SetShadowedImage(const gfx::ImageSkia& bitmap);
  // Override for custom initialization.
  virtual void Init();
  // Override to subclass IconView.
  virtual IconView* CreateIconView();
  IconView* icon_view() const { return icon_view_; }
  ShelfButtonHost* host() const { return host_; }

 private:
  class BarView;

  // Updates the parts of the button to reflect the current |state_| and
  // alignment. This may add or remove views, layout and paint.
  void UpdateState();

  // Updates the status bar (bitmap, orientation, visibility).
  void UpdateBar();

  ShelfButtonHost* host_;
  IconView* icon_view_;
  // Draws a bar underneath the image to represent the state of the application.
  BarView* bar_;
  // The current state of the application, multiple values of AppState are or'd
  // together.
  int state_;

  gfx::ShadowValues icon_shadows_;

  // If non-null the destuctor sets this to true. This is set while the menu is
  // showing and used to detect if the menu was deleted while running.
  bool* destroyed_flag_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButton);
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_BUTTON_H_
