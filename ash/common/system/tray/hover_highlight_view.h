// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include "ash/common/system/tray/actionable_view.h"
#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_constants.h"

namespace views {
class ImageView;
class Label;
class BoxLayout;
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

  // views::View
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name.
  void AddIconAndLabel(const gfx::ImageSkia& image,
                       const base::string16& text,
                       bool highlight);

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name. This method allows the indent and spacing between elements
  // to be set by the caller. |icon_size| is the size of the icon. |indent| is
  // the distance between the edges of the view and the icons, and
  // |space_between_items| is the minimum distance between any two child views.
  // All distances are in DP.
  void AddIconAndLabelCustomSize(const gfx::ImageSkia& image,
                                 const base::string16& text,
                                 bool highlight,
                                 int icon_size,
                                 int indent,
                                 int space_between_items);

  // Convenience function for adding a label with padding on the left for a
  // blank icon. This also sets the accessible name. Returns label after
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

  // Add an optional right icon to an already established view (call one of
  // the other Add* functions first). |icon_size| is the size of the icon in DP.
  void AddRightIcon(const gfx::ImageSkia& image, int icon_size);

  // Hide or show the right icon.
  void SetRightIconVisible(bool visible);

  // Allows view to expand its height.
  // Size of unexapandable view is fixed and equals to kTrayPopupItemHeight.
  void SetExpandable(bool expandable);

  // Enables or disable highlighting on the label, where a highlighted label
  // just uses a bold font.
  void SetHighlight(bool hightlight);

  // Set a custom height for the view. A value of 0 means that no custom height
  // is set.
  void set_custom_height(int custom_height) { custom_height_ = custom_height; }

  void set_highlight_color(SkColor color) { highlight_color_ = color; }
  void set_default_color(SkColor color) { default_color_ = color; }
  void set_text_highlight_color(SkColor c) { text_highlight_color_ = c; }
  void set_text_default_color(SkColor color) { text_default_color_ = color; }

  views::Label* text_label() { return text_label_; }

  bool hover() const { return hover_; }

  void set_tooltip(const base::string16& tooltip) { tooltip_ = tooltip; }

 protected:
  // Overridden from views::View.
  void GetAccessibleState(ui::AXViewState* state) override;

  // Sets the highlighted color on a text label if |hover| is set.
  void SetHoverHighlight(bool hover);

 private:
  // Actually adds the icon and label but does not set the layout manager
  void DoAddIconAndLabel(const gfx::ImageSkia& image,
                         int icon_size,
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

  ViewClickListener* listener_ = nullptr;
  views::Label* text_label_ = nullptr;
  views::BoxLayout* box_layout_ = nullptr;
  views::ImageView* right_icon_ = nullptr;
  SkColor highlight_color_ = 0;
  SkColor default_color_ = 0;
  SkColor text_highlight_color_ = 0;
  SkColor text_default_color_ = 0;
  bool hover_ = false;
  bool expandable_ = false;
  bool checkable_ = false;
  bool checked_ = false;
  int custom_height_ = 0;
  base::string16 tooltip_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
