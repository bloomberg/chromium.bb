// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
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

  // views::View:
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  // Convenience function for adding an icon and a label. This also sets the
  // accessible name. Primarily used for scrollable rows in detailed views.
  void AddIconAndLabel(const gfx::ImageSkia& image, const base::string16& text);

  // Convenience function for adding an icon, a main label, and a sub label.
  // This also sets the accessible name besed on the main label. Used for
  // scrollable rows in detailed views.
  void AddIconAndLabels(const gfx::ImageSkia& image,
                        const base::string16& text,
                        const base::string16& sub_text);

  // A convenience function for adding an icon and label for a system menu
  // default view row.
  void AddIconAndLabelForDefaultView(const gfx::ImageSkia& image,
                                     const base::string16& text);

  // Convenience function for adding a label with padding on the left for a
  // blank icon. This also sets the accessible name. Returns label after
  // parenting it.
  // TODO(tdanderson): Remove this function and use AddLabelRow() instead.
  // See crbug.com/708190.
  views::Label* AddLabelDeprecated(const base::string16& text,
                                   gfx::HorizontalAlignment alignment,
                                   bool highlight);

  // Adds a row containing only a text label, inset on the left by the
  // horizontal space that would normally be occupied by an icon.
  void AddLabelRow(const base::string16& text);

  // Add an optional right icon to an already established view (call one of
  // the other Add* functions first). |icon_size| is the size of the icon in DP.
  void AddRightIcon(const gfx::ImageSkia& image, int icon_size);

  // Add an optional right view to an already established view (call one of
  // the other Add* functions first).
  void AddRightView(views::View* view);

  // Hide or show the right view.
  void SetRightViewVisible(bool visible);

  // Allows view to expand its height. Size of unexapandable view is fixed and
  // equals to kTrayPopupItemHeight.
  void SetExpandable(bool expandable);

  // Set a custom height for the view. A value of 0 means that no custom height
  // is set.
  void set_custom_height(int custom_height) { custom_height_ = custom_height; }

  // Changes the view's current accessibility state. This will fire an
  // accessibility event if needed.
  void SetAccessiblityState(AccessibilityState accessibility_state);

  views::Label* text_label() { return text_label_; }
  views::Label* sub_text_label() { return sub_text_label_; }

  void set_tooltip(const base::string16& tooltip) { tooltip_ = tooltip; }

 protected:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  TriView* tri_view() { return tri_view_; }

 private:
  // Adds the image and label to the row with the label being styled using
  // |font_style|.
  void DoAddIconAndLabel(const gfx::ImageSkia& image,
                         const base::string16& text,
                         TrayPopupItemStyle::FontStyle font_style);

  // Adds the image, main label and sub label to the row with the main label
  // being styled using |font_style| and the sub label being styled using
  // FontStyle::CAPTION and ColorStyle::INACTIVE.
  void DoAddIconAndLabels(const gfx::ImageSkia& image,
                          const base::string16& text,
                          TrayPopupItemStyle::FontStyle font_style,
                          const base::string16& sub_text);

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnEnabledChanged() override;
  void OnFocus() override;

  ViewClickListener* listener_ = nullptr;
  views::Label* text_label_ = nullptr;
  views::Label* sub_text_label_ = nullptr;
  views::BoxLayout* box_layout_ = nullptr;
  views::ImageView* left_icon_ = nullptr;
  views::View* right_view_ = nullptr;
  TriView* tri_view_ = nullptr;
  SkColor text_default_color_ = 0;
  bool expandable_ = false;
  int custom_height_ = 0;
  AccessibilityState accessibility_state_ = AccessibilityState::DEFAULT;
  base::string16 tooltip_;

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
