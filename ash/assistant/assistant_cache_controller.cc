// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_cache_controller.h"

#include <vector>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AssistantCacheController::AssistantCacheController()
    : voice_interaction_binding_(this) {
  UpdateConversationStarters();

  // Bind to observe changes to screen context preference.
  mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_binding_.Bind(mojo::MakeRequest(&ptr));
  Shell::Get()->voice_interaction_controller()->AddObserver(std::move(ptr));
}

AssistantCacheController::~AssistantCacheController() = default;

void AssistantCacheController::AddModelObserver(
    AssistantCacheModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantCacheController::RemoveModelObserver(
    AssistantCacheModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantCacheController::OnVoiceInteractionContextEnabled(bool enabled) {
  UpdateConversationStarters();
}

// TODO(dmblack): The conversation starter cache should receive its contents
// from the server. Hard-coding for the time being.
void AssistantCacheController::UpdateConversationStarters() {
  using namespace chromeos::assistant::mojom;

  std::vector<AssistantSuggestionPtr> conversation_starters;

  auto AddConversationStarter = [&conversation_starters](int message_id) {
    AssistantSuggestionPtr starter = AssistantSuggestion::New();
    starter->text = l10n_util::GetStringUTF8(message_id);
    conversation_starters.push_back(std::move(starter));
  };

  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHAT_CAN_YOU_DO);

  // TODO(dmblack): Add a deep link for this chip to start a screen context
  // query to receive back contextual cards.
  if (Shell::Get()->voice_interaction_controller()->context_enabled())
    AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN);

  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_CALENDAR);

  // TODO(dmblack): Shuffle these chips and limit our total chip count to four.
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_PLAY_MUSIC);
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_SEND_AN_EMAIL);
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHATS_THE_WEATHER);
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_IM_BORED);
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_OPEN_FILES);

  model_.SetConversationStarters(std::move(conversation_starters));
}

}  // namespace ash
