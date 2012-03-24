// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#define ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#pragma once

#include "ui/gfx/font.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

class SkBitmap;

namespace views {
class Label;
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

class ViewClickListener {
 public:
  virtual ~ViewClickListener() {}
  virtual void ClickedOn(views::View* sender) = 0;
};

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click.
class HoverHighlightView : public views::View {
 public:
  explicit HoverHighlightView(ViewClickListener* listener);
  virtual ~HoverHighlightView();

  // Convenience function for adding an icon and a label.
  void AddIconAndLabel(const SkBitmap& image,
                       const string16& text,
                       gfx::Font::FontStyle style);

  // Convenience function for adding a label with padding on the left for a
  // blank icon.
  void AddLabel(const string16& text, gfx::Font::FontStyle style);

  // Set the accessible name.  Should be used if this doesn't match the label.
  void SetAccessibleName(const string16& name);

 private:
  // Overridden from views::View.
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  ViewClickListener* listener_;
  string16 accessible_name_;

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
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  gfx::Size fixed_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedScrollView);
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
