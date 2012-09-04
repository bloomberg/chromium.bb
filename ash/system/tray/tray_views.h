// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#define ASH_SYSTEM_TRAY_TRAY_VIEWS_H_

#include "ash/ash_export.h"
#include "ash/wm/shelf_types.h"
#include "ui/gfx/font.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

typedef unsigned int SkColor;

namespace gfx {
class ImageSkia;
}

namespace views {
class Label;
class BoxLayout;
}

namespace ash {
namespace internal {

class TrayItemView;

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
// enter or space when focused. Note that the action is triggered on mouse-up,
// instead of on mouse-down. So if user presses the mouse on the view, then
// moves the mouse out of the view and then releases, then the action will not
// be performed.
// Exported for SystemTray.
class ASH_EXPORT ActionableView : public views::View {
 public:
  ActionableView();

  virtual ~ActionableView();

  // Set the accessible name.
  void SetAccessibleName(const string16& name);
  const string16& accessible_name() const { return accessible_name_; }

 protected:
  void DrawBorder(gfx::Canvas* canvas, const gfx::Rect& bounds);

  // Performs an action when user clicks on the view (on mouse-press event), or
  // presses a key when this view is in focus. Returns true if the event has
  // been handled and an action was performed. Returns false otherwise.
  virtual bool PerformAction(const ui::Event& event) = 0;

  // Overridden from views::View.
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual ui::EventResult OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

 private:
  string16 accessible_name_;
  bool has_capture_;

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
  void AddIconAndLabel(const gfx::ImageSkia& image,
                       const string16& text,
                       gfx::Font::FontStyle style);

  // Convenience function for adding a label with padding on the left for a
  // blank icon.  This also sets the accessible name.
  void AddLabel(const string16& text, gfx::Font::FontStyle style);

  void set_highlight_color(SkColor color) { highlight_color_ = color; }
  void set_default_color(SkColor color) { default_color_ = color; }
  void set_text_highlight_color(SkColor c) { text_highlight_color_ = c; }
  void set_text_default_color(SkColor color) { text_default_color_ = color; }
  void set_fixed_height(int height) { fixed_height_ = height; }

 private:
  // Overridden from ActionableView.
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
  // Change the fixed size of the view. Invalidates the layout (by calling
  // PreferredSizeChanged()).
  void SetFixedSize(const gfx::Size& size);

  void set_fixed_size(const gfx::Size& size) { fixed_size_ = size; }

  // views::View public method overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;

 protected:
  // views::View protected method overrides.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

 private:
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
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
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
                        int disabled_resource_id,
                        int enabled_resource_id_hover,
                        int disabled_resource_id_hover,
                        int accessible_name_id);
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

// A button with a bar image and title text below the bar image. These buttons
// will be used in audio and brightness control UI, which can be toggled with
// on/off states.
class TrayBarButtonWithTitle : public views::CustomButton {
 public:
  TrayBarButtonWithTitle(views::ButtonListener* listener,
                                  int title_id,
                                  int width);
  virtual ~TrayBarButtonWithTitle();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  void UpdateButton(bool control_on);

 private:
  class TrayBarButton;

  TrayBarButton* image_;
  views::Label* title_;
  int width_;
  int image_height_;

  DISALLOW_COPY_AND_ASSIGN(TrayBarButtonWithTitle);
};

// The 'special' looking row in the uber-tray popups. This is usually the bottom
// row in the popups, and has a fixed height.
class SpecialPopupRow : public views::View {
 public:
  SpecialPopupRow();
  virtual ~SpecialPopupRow();

  void SetTextLabel(int string_id, ViewClickListener* listener);
  void SetContent(views::View* view);

  void AddButton(TrayPopupHeaderButton* button);

  views::View* content() const { return content_; }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  views::View* content_;
  views::View* button_container_;
  views::Label* text_label_;
  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRow);
};

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

// TODO(jennyz): refactor these two functions to SystemTrayItem.
// Sets the empty border of an image tray item for adjusting the space
// around it.
void SetTrayImageItemBorder(views::View* tray_view, ShelfAlignment alignment);
// Sets the empty border around a label tray item for adjusting the space
// around it.
void SetTrayLabelItemBorder(TrayItemView* tray_view,
                            ShelfAlignment alignment);

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
