// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#define ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#pragma once

#include "ui/gfx/font.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class SkBitmap;

namespace views {
class Label;
}

namespace ash {
namespace internal {

// An image view with that always has a fixed width (kTrayPopupDetailsIconWidth)
class FixedWidthImageView : public views::ImageView {
 public:
  FixedWidthImageView();
  virtual ~FixedWidthImageView();

 private:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FixedWidthImageView);
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
  void AddLabel(const string16& text);

 private:
  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  ViewClickListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
