// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/controls/button/button.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {

// TODO(dmblack): Move to /ash/assistant/ui/base/.
// View representing a suggestion chip.
class COMPONENT_EXPORT(ASSISTANT_UI) SuggestionChipView : public views::Button {
 public:
  // Initialization parameters.
  struct Params {
    Params();
    ~Params();

    // Display text.
    base::string16 text;
    // Optional icon.
    base::Optional<gfx::ImageSkia> icon;
  };

  SuggestionChipView(const Params& params, views::ButtonListener* listener);
  ~SuggestionChipView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  void SetIcon(const gfx::ImageSkia& icon);

  void SetText(const base::string16& text);
  const base::string16& GetText() const;

 private:
  void InitLayout(const Params& params);

  views::ImageView* icon_view_;  // Owned by view hierarchy.
  views::Label* text_view_;      // Owned by view hierarchy.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
