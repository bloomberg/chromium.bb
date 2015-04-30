// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include "ash/system/tray/actionable_view.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_constants.h"

namespace views {
class Label;
}

namespace ash {
class ViewClickListener;

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click. The view can also be forced to
// maintain a fixed height.
class HoverHighlightView : public ActionableView {
 public:
  explicit HoverHighlightView(ViewClickListener* listener);
  ~HoverHighlightView() override;

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name.
  void AddIconAndLabel(const gfx::ImageSkia& image,
                       const base::string16& text,
                       bool highlight);

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name. The icon has an indent equal to
  // kTrayPopupPaddingHorizontal.
  void AddIndentedIconAndLabel(const gfx::ImageSkia& image,
                               const base::string16& text,
                               bool highlight);

  // Convenience function for adding a label with padding on the left for a
  // blank icon.  This also sets the accessible name. Returns label after
  // parenting it.
  views::Label* AddLabel(const base::string16& text,
                         gfx::HorizontalAlignment alignment,
                         bool highlight);

  // Convenience function for adding an optional check and a label. In the
  // absence of a check, padding is added to align with checked items.
  // Returns label after parenting it.
  views::Label* AddCheckableLabel(const base::string16& text,
                                  bool highlight,
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

 protected:
  // Overridden from views::View.
  void GetAccessibleState(ui::AXViewState* state) override;

  // Sets the highlighted color on a text label if |hover| is set.
  void SetHoverHighlight(bool hover);

 private:
  // Actually adds the icon and label but does not set the layout manager
  void DoAddIconAndLabel(const gfx::ImageSkia& image,
                         const base::string16& text,
                         bool highlight);

  // Overridden from ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnEnabledChanged() override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;

  ViewClickListener* listener_;
  views::Label* text_label_;
  SkColor highlight_color_;
  SkColor default_color_;
  SkColor text_highlight_color_;
  SkColor text_default_color_;
  bool hover_;
  bool expandable_;
  bool checkable_;
  bool checked_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
