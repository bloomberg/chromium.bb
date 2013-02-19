// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include "ash/system/tray/actionable_view.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/font.h"

namespace views {
class Label;
}

namespace ash {
namespace internal {

class ViewClickListener;

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click. The view can also be forced to
// maintain a fixed height.
class HoverHighlightView : public ActionableView {
 public:
  explicit HoverHighlightView(ViewClickListener* listener);
  virtual ~HoverHighlightView();

  // Convenience function for adding an icon and a label.  This also sets the
  // accessible name.
  void AddIconAndLabel(const gfx::ImageSkia& image,
                       const string16& text,
                       gfx::Font::FontStyle style);

  // Convenience function for adding a label with padding on the left for a
  // blank icon.  This also sets the accessible name.
  // Returns label after parenting it.
  views::Label* AddLabel(const string16& text, gfx::Font::FontStyle style);

  // Convenience function for adding an optional check and a label.  In the
  // absence of a check, padding is added to align with checked items.
  // Returns label after parenting it.
  views::Label* AddCheckableLabel(const string16& text,
                                  gfx::Font::FontStyle style,
                                  bool checked);

  // Allows view to expand its height.
  // Size of unexapandable view is fixed and equals to kTrayPopupItemHeight.
  void SetExpandable(bool expandable);

  void set_highlight_color(SkColor color) { highlight_color_ = color; }
  void set_default_color(SkColor color) { default_color_ = color; }
  void set_text_highlight_color(SkColor c) { text_highlight_color_ = c; }
  void set_text_default_color(SkColor color) { text_default_color_ = color; }

  views::Label* text_label() { return text_label_; }

  bool hover() const { return hover_; }

 private:
  // Overridden from ActionableView:
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  ViewClickListener* listener_;
  views::Label* text_label_;
  SkColor highlight_color_;
  SkColor default_color_;
  SkColor text_highlight_color_;
  SkColor text_default_color_;
  bool hover_;
  bool expandable_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
