// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_

#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"

namespace gfx {
enum ElideBehavior;
class ImageSkia;
}  // namespace gfx

namespace views {
class ButtonListener;
class Label;
class StyledLabel;
class View;
}  // namespace views

// A button taking the full width of its parent that shows a background color
// when hovered over.
class HoverButton : public views::LabelButton {
 public:
  // Creates a single line hover button with no icon.
  HoverButton(views::ButtonListener* button_listener,
              const base::string16& text);

  // Creates a single line hover button with an icon.
  HoverButton(views::ButtonListener* button_listener,
              const gfx::ImageSkia& icon,
              const base::string16& text);

  // Creates a HoverButton with custom subviews. |icon_view| replaces the
  // LabelButton icon, and titles appear on separate rows. An empty |subtitle|
  // will vertically center |title|.
  HoverButton(views::ButtonListener* button_listener,
              std::unique_ptr<views::View> icon_view,
              const base::string16& title,
              const base::string16& subtitle);

  ~HoverButton() override;

  // Updates the title text, and applies the secondary style to the text
  // specified by |range|. If |range| is invalid, no style is applied. This
  // method is only supported for |HoverButton|s created with a title and
  // subtitle.
  void SetTitleTextWithHintRange(const base::string16& title_text,
                                 const gfx::Range& range);

  // This method is only supported for |HoverButton|s created with a title and
  // non-empty subtitle.
  void SetSubtitleElideBehavior(gfx::ElideBehavior elide_behavior);

 protected:
  // views::LabelButton:
  void StateChanged(ButtonState old_state) override;
  bool ShouldUseFloodFillInkDrop() const override;

  // views::InkDropHostView:
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  // views::View:
  void Layout() override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  views::StyledLabel* title_;
  views::Label* subtitle_;

  // The horizontal space the padding and icon take up. Used for calculating the
  // available space for |title_|, if it exists.
  int taken_width_ = 0;

  DISALLOW_COPY_AND_ASSIGN(HoverButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_
