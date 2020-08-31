// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_

#include <string>

#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantTextElement;

// AssistantTextElementView is the visual representation of an
// AssistantTextElement. It is a child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantTextElementView
    : public AssistantUiElementView {
 public:
  explicit AssistantTextElementView(const AssistantTextElement* text_element);
  ~AssistantTextElementView() override;

  // AssistantUiElementView:
  const char* GetClassName() const override;
  ui::Layer* GetLayerForAnimating() override;
  std::string ToStringForTesting() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  void InitLayout(const AssistantTextElement* text_element);

  views::Label* label_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantTextElementView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_
