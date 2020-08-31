// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_text_element.h"

#include "ash/assistant/ui/assistant_ui_constants.h"

namespace ash {

AssistantTextElement::AssistantTextElement(const std::string& text)
    : AssistantUiElement(AssistantUiElementType::kText), text_(text) {}

AssistantTextElement::~AssistantTextElement() = default;

}  // namespace ash
