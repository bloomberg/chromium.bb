// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// Enumeration of Assistant button ID. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if more buttons will be added.
enum class AssistantButtonId {
  kBack = 1,
  kClose = 2,
  kMinimize = 3,
  kKeyboardInputToggle = 4,
  kVoiceInputToggle = 5,
  kSettings = 6,
  kMaxValue = kSettings,
};

class AssistantButton : public views::ImageButton,
                        public views::ButtonListener {
 public:
  AssistantButton(views::ButtonListener* listener, AssistantButtonId button_id);
  ~AssistantButton() override;

  // views::Button:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  views::ButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(AssistantButton);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
