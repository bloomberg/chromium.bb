// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

class AssistantUiElement;

// Models a renderable Assistant response.
class AssistantResponse {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;

  AssistantResponse();
  ~AssistantResponse();

  // Adds the specified |ui_element| that should be rendered for the
  // interaction.
  void AddUiElement(std::unique_ptr<AssistantUiElement> ui_element);

  // Returns all UI elements belonging to the response.
  const std::vector<std::unique_ptr<AssistantUiElement>>& GetUiElements() const;

  // Adds the specified |suggestions| that should be rendered for the
  // interaction.
  void AddSuggestions(std::vector<AssistantSuggestionPtr> suggestions);

  // Returns the suggestion uniquely identified by |id|.
  const AssistantSuggestion* GetSuggestionById(int id) const;

  // Returns all suggestions belongs to the response, mapped to a unique id.
  std::map<int, const AssistantSuggestion*> GetSuggestions() const;

 private:
  std::vector<std::unique_ptr<AssistantUiElement>> ui_elements_;
  std::vector<AssistantSuggestionPtr> suggestions_;

  DISALLOW_COPY_AND_ASSIGN(AssistantResponse);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
