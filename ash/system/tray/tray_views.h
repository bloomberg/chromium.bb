// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#define ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#pragma once

#include "ash/ash_export.h"
#include "ui/gfx/font.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

class SkBitmap;
typedef unsigned int SkColor;

namespace views {
class Label;
class BoxLayout;
}

namespace ash {
namespace internal {

// An image view with a specified width and height (kTrayPopupDetailsIconWidth).
// If the specified width or height is zero, then the image size is used for
// that dimension.
class FixedSizedImageView : public views::ImageView {
 public:
  FixedSizedImageView(int width, int height);
  virtual ~FixedSizedImageView();

 private:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  int width_;
  int height_;
  DISALLOW_COPY_AND_ASSIGN(FixedSizedImageView);
};

// A focusable view that performs an action when user clicks on it, or presses
// enter or space when focused. Exported for SystemTray.
class ASH_EXPORT ActionableView : public views::View {
 public:
  ActionableView();

  virtual ~ActionableView();

  // Set the accessible name.
  void SetAccessibleName(const string16& name);
  const string16& accessible_name() const { return accessible_name_; }

 protected:
  // Performs an action when user clicks on the view (on mouse-press event), or
  // presses a key when this view is in focus. Returns true if the event has
  // been handled and an action was performed. Returns false otherwise.
  virtual bool PerformAction(const views::Event& event) = 0;

  // Overridden from views::View.
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

 private:
  string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(ActionableView);
};

class ViewClickListener {
 public:
  virtual ~ViewClickListener() {}
  virtual void ClickedOn(views::View* sender) = 0;
};

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click. The view can also be forced to
// maintain a fixed height.
class HoverHighlightView : public ActionableView {
 public:
  explicit HoverHighlightView(ViewClickListener* listener);
  virtual ~HoverHighlightView();

  // Convenience function for adding an icon and a label.  This also sets the
  // accessible name.
  void AddIconAndLabel(const SkBitmap& image,
                       const string16& text,
                       gfx::Font::FontStyle style);

  // Convenience function for adding a label with padding on the left for a
  // blank icon.  This also sets the accessible name.
  void AddLabel(const string16& text, gfx::Font::FontStyle style);

  void set_highlight_color(SkColor color) { highlight_color_ = color; }
  void set_default_color(SkColor color) { default_color_ = color; }
  void set_fixed_height(int height) { fixed_height_ = height; }

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  ViewClickListener* listener_;
  SkColor highlight_color_;
  SkColor default_color_;
  int fixed_height_;
  bool hover_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

// A custom scroll-view that has a specified dimension.
class FixedSizedScrollView : public views::ScrollView {
 public:
  FixedSizedScrollView();

  virtual ~FixedSizedScrollView();

  void SetContentsView(View* view);

  void set_fixed_size(gfx::Size size) { fixed_size_ = size; }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  gfx::Size fixed_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedScrollView);
};

// A custom textbutton with some extra vertical padding, and custom border,
// alignment and hover-effects.
class TrayPopupTextButton : public views::TextButton {
 public:
  TrayPopupTextButton(views::ButtonListener* listener, const string16& text);
  virtual ~TrayPopupTextButton();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  bool hover_;
  scoped_ptr<views::Background> hover_bg_;
  scoped_ptr<views::Border> hover_border_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupTextButton);
};

// A container for TrayPopupTextButtons (and possibly other views). This sets up
// the TrayPopupTextButtons to paint their borders correctly.
class TrayPopupTextButtonContainer : public views::View {
 public:
  TrayPopupTextButtonContainer();
  virtual ~TrayPopupTextButtonContainer();

  void AddTextButton(TrayPopupTextButton* button);

  views::BoxLayout* layout() const { return layout_; }

 private:
  views::BoxLayout* layout_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupTextButtonContainer);
};

// A ToggleImageButton with fixed size, paddings and hover effects. These
// buttons are used in the header.
class TrayPopupHeaderButton : public views::ToggleImageButton {
 public:
  TrayPopupHeaderButton(views::ButtonListener* listener,
                        int enabled_resource_id,
                        int disabled_resource_id);
  virtual ~TrayPopupHeaderButton();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::CustomButton.
  virtual void StateChanged() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupHeaderButton);
};

// Creates a container for the various detailed popups. Clicking on the view
// triggers the callback in ViewClickListener.
views::View* CreateDetailedHeaderEntry(int string_id,
                                       ViewClickListener* listener);

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
