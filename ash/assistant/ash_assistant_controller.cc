// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ash_assistant_controller.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"

namespace ash {

AshAssistantController::AshAssistantController()
    : assistant_controller_binding_(this),
      assistant_event_subscriber_binding_(this) {}

AshAssistantController::~AshAssistantController() {
  assistant_controller_binding_.Close();
  assistant_event_subscriber_binding_.Close();
}

void AshAssistantController::BindRequest(
    mojom::AshAssistantControllerRequest request) {
  assistant_controller_binding_.Bind(std::move(request));
}

void AshAssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  // Subscribe to Assistant events.
  chromeos::assistant::mojom::AssistantEventSubscriberPtr ptr;
  assistant_event_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant->AddAssistantEventSubscriber(std::move(ptr));
}

void AshAssistantController::OnHtmlResponse(const std::string& response) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnSuggestionsResponse(
    const std::vector<std::string>& response) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnTextResponse(const std::string& response) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnSpeechLevelUpdated(float speech_level) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnOpenUrlResponse(const GURL& url) {
  Shell::Get()->shell_delegate()->OpenUrlFromArc(url);
}

}  // namespace ash
