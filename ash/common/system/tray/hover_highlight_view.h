// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_constants.h"

namespace views {
class ImageView;
class Label;
class BoxLayout;
}

namespace ash {
class TriView;
class ViewClickListener;

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click. The view can also be forced to
// maintain a fixed height.
class HoverHighlightView : public ActionableView {
 public:
  enum class AccessibilityState {
    // The default accessibility view.
    DEFAULT,
    // This view is a checked checkbox.
    CHECKED_CHECKBOX,
    // This view is an unchecked checkbox.
    UNCHECKED_CHECKBOX
  };

  explicit HoverHighlightView(ViewClickListener* listener);
  ~HoverHighlightView() override;

  // views::View
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name. Primarily used for scrollable rows in detailed views.
  void AddIconAndLabel(const gfx::ImageSkia& image,
                       const base::string16& text,
                       bool highlight);

  // Convenience function for adding an icon, a main label, and a sub label.
  // This also sets the accessible name besed on the main label. Used for
  // scrollable rows in detailed views in material design.
  void AddIconAndLabels(const gfx::ImageSkia& image,
                        const base::string16& text,
                        const base::string16& sub_text);

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name. This method allows the indent and spacing between elements
  // to be set by the caller. |icon_size| is the size of the icon. |indent| is
  // the distance between the edges of the view and the icons, and
  // |space_between_items| is the minimum distance between any two child views.
  // All distances are in DP. Primarily used for scrollable rows in detailed
  // views.
  void AddIconAndLabelCustomSize(const gfx::ImageSkia& image,
                                 const base::string16& text,
                                 bool highlight,
                                 int icon_size,
                                 int indent,
                                 int space_between_items);

  // A convenience function for adding an icon and label for a system menu
  // default view row.
  void AddIconAndLabelForDefaultView(const gfx::ImageSkia& image,
                                     const base::string16& text,
                                     bool highlight);

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

  // Adds a row containing only a text label, inset on the left by the
  // horizontal space that would normally be occupied by an icon.
  void AddLabelRowMd(const base::string16& text);

  // Add an optional right icon to an already established view (call one of
  // the other Add* functions first). |icon_size| is the size of the icon in DP.
  void AddRightIcon(const gfx::ImageSkia& image, int icon_size);

  // Add an optional right view to an already established view (call one of
  // the other Add* functions first).
  void AddRightView(views::View* view);

  // Hide or show the right view.
  void SetRightViewVisible(bool visible);

  // Allows view to expand its height.
  // Size of unexapandable view is fixed and equals to kTrayPopupItemHeight.
  void SetExpandable(bool expandable);

  // Enables or disable highlighting on the label, where a highlighted label
  // just uses a bold font.
  void SetHighlight(bool hightlight);

  // Set a custom height for the view. A value of 0 means that no custom height
  // is set.
  void set_custom_height(int custom_height) { custom_height_ = custom_height; }

  // Changes the view's current accessibility state. This will fire an
  // accessibility event if needed.
  void SetAccessiblityState(AccessibilityState accessibility_state);

  void set_highlight_color(SkColor color) { highlight_color_ = color; }
  void set_default_color(SkColor color) { default_color_ = color; }
  void set_text_highlight_color(SkColor c) { text_highlight_color_ = c; }
  void set_text_default_color(SkColor color) { text_default_color_ = color; }

  views::Label* text_label() { return text_label_; }
  views::Label* sub_text_label() { return sub_text_label_; }

  bool hover() const { return hover_; }

  void set_tooltip(const base::string16& tooltip) { tooltip_ = tooltip; }

 protected:
  // Overridden from views::View.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Sets the highlighted color on a text label if |hover| is set.
  void SetHoverHighlight(bool hover);

  TriView* tri_view() { return tri_view_; }

 private:
  // Actually adds the icon and label but does not set the layout manager.
  // Not used in material design.
  void DoAddIconAndLabel(const gfx::ImageSkia& image,
                         int icon_size,
                         const base::string16& text,
                         bool highlight);

  // Adds the image and label to the row with the label being styled using
  // |font_style|. Only used in material design.
  void DoAddIconAndLabelMd(const gfx::ImageSkia& image,
                           const base::string16& text,
                           TrayPopupItemStyle::FontStyle font_style);

  // Adds the image, main label and sub label to the row with the main label
  // being styled using |font_style| and the sub label being styled using
  // FontStyle::CAPTION and ColorStyle::INACTIVE. Only used in material design.
  void DoAddIconAndLabelsMd(const gfx::ImageSkia& image,
                            const base::string16& text,
                            TrayPopupItemStyle::FontStyle font_style,
                            const base::string16& sub_text);

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
  views::Label* sub_text_label_ = nullptr;
  views::BoxLayout* box_layout_ = nullptr;  // Not used in material design.
  views::ImageView* left_icon_ = nullptr;
  views::View* right_view_ = nullptr;
  TriView* tri_view_ = nullptr;  // Only used in material design.
  SkColor highlight_color_ = 0;  // Not used in material design.
  SkColor default_color_ = 0;
  SkColor text_highlight_color_ = 0;  // Not used in material design.
  SkColor text_default_color_ = 0;    // Not used in material design.
  bool hover_ = false;                // Not used in material design.
  bool expandable_ = false;
  int custom_height_ = 0;  // Not used in material design.
  AccessibilityState accessibility_state_ = AccessibilityState::DEFAULT;
  base::string16 tooltip_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
