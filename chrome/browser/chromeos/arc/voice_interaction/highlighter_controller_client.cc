// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/highlighter_controller_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {

namespace {
constexpr int kSelectionReportDelayMs = 600;
}

HighlighterControllerClient::HighlighterControllerClient(
    ArcVoiceInteractionFrameworkService* service)
    : binding_(this), service_(service) {}

HighlighterControllerClient::~HighlighterControllerClient() = default;

void HighlighterControllerClient::Attach() {
  ConnectToHighlighterController();
  ash::mojom::HighlighterControllerClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  highlighter_controller_->SetClient(std::move(client));
}

void HighlighterControllerClient::Detach() {
  binding_.Close();
}

void HighlighterControllerClient::SetControllerForTesting(
    ash::mojom::HighlighterControllerPtr controller) {
  highlighter_controller_ = std::move(controller);
}

void HighlighterControllerClient::SimulateSelectionTimeoutForTesting() {
  DCHECK(delay_timer_ && delay_timer_->IsRunning());
  delay_timer_->user_task().Run();
  delay_timer_.reset();
}

void HighlighterControllerClient::FlushMojoForTesting() {
  highlighter_controller_.FlushForTesting();
}

void HighlighterControllerClient::ConnectToHighlighterController() {
  // Tests may bind to their own HighlighterController.
  if (highlighter_controller_)
    return;

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &highlighter_controller_);
}

void HighlighterControllerClient::HandleSelection(const gfx::Rect& rect) {
  // Delay the actual voice interaction service invocation for better
  // visual synchronization with the metalayer animation.
  delay_timer_ = std::make_unique<base::Timer>(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kSelectionReportDelayMs),
      base::Bind(&HighlighterControllerClient::ReportSelection,
                 base::Unretained(this), rect),
      false /* is_repeating */);
  delay_timer_->Reset();
}

void HighlighterControllerClient::HandleEnabledStateChange(bool enabled) {
  // ArcVoiceInteractionFrameworkService::HideMetalayer() causes the container
  // to show a toast-like prompt. This toast is redundant and causes
  // unnecessary flicker if the full voice interaction UI is about to be
  // displayed soon. |start_session_pending| is a good signal that the session
  // is about to start, but it is not guaranteed: 1) The user might re-enter the
  // metalayer mode before the timer fires.
  //    In this case the container will keep showing the prompt for the
  //    metalayer mode.
  // 2) The session might fail to start due to a peculiar combination of
  //    failures on the way to the voice interaction UI. This is an open
  //    problem.
  if (enabled)
    service_->ShowMetalayer();
  else if (!start_session_pending())
    service_->HideMetalayer();
}

void HighlighterControllerClient::ReportSelection(const gfx::Rect& rect) {
  service_->StartSessionFromUserInteraction(rect);
}

}  // namespace arc
