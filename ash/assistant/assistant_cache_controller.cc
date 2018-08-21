// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_cache_controller.h"

#include <vector>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/rand_util.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Conversation starters.
constexpr int kNumOfConversationStarters = 3;

}  // namespace

AssistantCacheController::AssistantCacheController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      voice_interaction_binding_(this) {
  UpdateConversationStarters();

  assistant_controller_->AddObserver(this);

  // Bind to observe changes to screen context preference.
  mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_binding_.Bind(mojo::MakeRequest(&ptr));
  Shell::Get()->voice_interaction_controller()->AddObserver(std::move(ptr));
}

AssistantCacheController::~AssistantCacheController() {
  assistant_controller_->RemoveObserver(this);
}

void AssistantCacheController::AddModelObserver(
    AssistantCacheModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantCacheController::RemoveModelObserver(
    AssistantCacheModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantCacheController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantCacheController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantCacheController::OnUiVisibilityChanged(bool visible,
                                                     AssistantSource source) {
  // When hiding the UI we update our cache of conversation starters so that
  // they're fresh for the next session.
  if (!visible)
    UpdateConversationStarters();
}

void AssistantCacheController::OnVoiceInteractionContextEnabled(bool enabled) {
  UpdateConversationStarters();
}

// TODO(dmblack): The conversation starter cache should receive its contents
// from the server. Hard-coding for the time being.
void AssistantCacheController::UpdateConversationStarters() {
  using namespace chromeos::assistant::mojom;

  std::vector<AssistantSuggestionPtr> conversation_starters;

  auto AddConversationStarter = [&conversation_starters](
                                    int message_id, GURL action_url = GURL()) {
    AssistantSuggestionPtr starter = AssistantSuggestion::New();
    starter->text = l10n_util::GetStringUTF8(message_id);
    starter->action_url = action_url;
    conversation_starters.push_back(std::move(starter));
  };

  // Always show the "What can you do?" conversation starter.
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHAT_CAN_YOU_DO);

  // Always show the "What's on my screen?" conversation starter (if enabled).
  if (Shell::Get()->voice_interaction_controller()->context_enabled()) {
    AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN,
                           assistant::util::CreateWhatsOnMyScreenDeepLink());
  }

  // The rest of the conversation starters will be shuffled...
  int shuffled_message_ids[] = {
      IDS_ASH_ASSISTANT_CHIP_IM_BORED,
      IDS_ASH_ASSISTANT_CHIP_OPEN_FILES,
      IDS_ASH_ASSISTANT_CHIP_PLAY_MUSIC,
      IDS_ASH_ASSISTANT_CHIP_SEND_AN_EMAIL,
      IDS_ASH_ASSISTANT_CHIP_SET_A_REMINDER,
      IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_CALENDAR,
      IDS_ASH_ASSISTANT_CHIP_WHATS_THE_WEATHER,
  };

  base::RandomShuffle(std::begin(shuffled_message_ids),
                      std::end(shuffled_message_ids));

  // ...and will be added until we reach |kNumOfConversationStarters|.
  for (int i = 0; conversation_starters.size() < kNumOfConversationStarters;
       ++i) {
    AddConversationStarter(shuffled_message_ids[i]);
  }

  model_.SetConversationStarters(std::move(conversation_starters));
}

}  // namespace ash
