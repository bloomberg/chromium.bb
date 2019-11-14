// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_proactive_suggestions_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_suggestions_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/proactive_suggestions_view.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/assistant/util/histogram_util.h"
#include "chromeos/services/assistant/public/features.h"
#include "ui/views/widget/widget.h"

namespace ash {

AssistantProactiveSuggestionsController::
    AssistantProactiveSuggestionsController(
        AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  DCHECK(chromeos::assistant::features::IsProactiveSuggestionsEnabled());
  assistant_controller_->AddObserver(this);
}

AssistantProactiveSuggestionsController::
    ~AssistantProactiveSuggestionsController() {
  auto* client = ProactiveSuggestionsClient::Get();
  if (client)
    client->SetDelegate(nullptr);

  assistant_controller_->RemoveObserver(this);
}

void AssistantProactiveSuggestionsController::
    OnAssistantControllerConstructed() {
  assistant_controller_->suggestions_controller()->AddModelObserver(this);
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantProactiveSuggestionsController::
    OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
  assistant_controller_->suggestions_controller()->RemoveModelObserver(this);
}

void AssistantProactiveSuggestionsController::OnAssistantReady() {
  // The proactive suggestions client initializes late so we need to wait for
  // the ready signal before binding as its delegate.
  auto* client = ProactiveSuggestionsClient::Get();
  if (client)
    client->SetDelegate(this);
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsClientDestroying() {
  auto* client = ProactiveSuggestionsClient::Get();
  if (client)
    client->SetDelegate(nullptr);
}

void AssistantProactiveSuggestionsController::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {
  // This method is invoked from the ProactiveSuggestionsClient to inform us
  // that the active set of proactive suggestions has changed. Because we have
  // read-only access to the Assistant suggestions model, we forward this event
  // to the Assistant suggestions controller to cache state. We will then react
  // to the change in model state in OnProactiveSuggestionsChanged(new, old).
  assistant_controller_->suggestions_controller()
      ->OnProactiveSuggestionsChanged(std::move(proactive_suggestions));
}

void AssistantProactiveSuggestionsController::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions,
    scoped_refptr<const ProactiveSuggestions> old_proactive_suggestions) {
  // If a set of earlier proactive suggestions had been shown, it's safe to
  // assume they are now being closed due to a change in context. Note that this
  // will no-op if the proactive suggestions view does not exist.
  CloseUi(ProactiveSuggestionsShowResult::kCloseByContextChange);

  if (!proactive_suggestions)
    return;

  // If this set of proactive suggestions is blacklisted, it should not be shown
  // to the user. A set of proactive suggestions may be blacklisted as a result
  // of duplicate suppression or as a result of the user explicitly closing the
  // proactive suggestions view.
  if (base::Contains(proactive_suggestions_blacklist_,
                     proactive_suggestions->hash())) {
    RecordProactiveSuggestionsShowAttempt(
        proactive_suggestions->category(),
        ProactiveSuggestionsShowAttempt::kAbortedByDuplicateSuppression);
    return;
  }

  ShowUi();
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsCloseButtonPressed() {
  // When the user explicitly closes the proactive suggestions view, that's a
  // strong signal that they don't want to see the same suggestions again so we
  // add it to our blacklist.
  proactive_suggestions_blacklist_.emplace(
      view_->proactive_suggestions()->hash());

  CloseUi(ProactiveSuggestionsShowResult::kCloseByUser);
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsViewHoverChanged(bool is_hovering) {
  // Hover changed events may occur during the proactive suggestion view's
  // closing sequence. When this occurs, no further action is necessary.
  if (!view_ || !view_->GetWidget() || view_->GetWidget()->IsClosed())
    return;

  if (!is_hovering) {
    // When the user is no longer hovering over the proactive suggestions view
    // we need to reset the timer so that it will auto-close appropriately.
    auto_close_timer_.Reset();
    return;
  }

  const base::TimeDelta remaining_time =
      auto_close_timer_.desired_run_time() - base::TimeTicks::Now();

  // The user is now hovering over the proactive suggestions view so we need to
  // pause the auto-close timer until we are no longer in a hovering state. Once
  // we leave hovering state, we will resume the auto-close timer with whatever
  // |remaining_time| is left. To accomplish this, we schedule the auto-close
  // timer to fire in the future...
  auto_close_timer_.Start(FROM_HERE, remaining_time,
                          auto_close_timer_.user_task());

  // ...but immediately stop it so that when we reset the auto-close timer upon
  // leaving hovering state, the timer will appropriately fire only after the
  // |remaining_time| has elapsed.
  auto_close_timer_.Stop();
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsViewPressed() {
  CloseUi(ProactiveSuggestionsShowResult::kClick);

  // Clicking on the proactive suggestions view causes the user to enter
  // Assistant UI where a proactive suggestions interaction will be initiated.
  assistant_controller_->ui_controller()->ShowUi(
      AssistantEntryPoint::kProactiveSuggestions);
}

void AssistantProactiveSuggestionsController::ShowUi() {
  DCHECK(!view_);
  view_ = new ProactiveSuggestionsView(assistant_controller_->view_delegate());
  view_->GetWidget()->ShowInactive();

  RecordProactiveSuggestionsShowAttempt(
      view_->proactive_suggestions()->category(),
      ProactiveSuggestionsShowAttempt::kSuccess);

  // If duplicate suppression is enabled, the user should not be presented with
  // this set of proactive suggestions again so we add it to our blacklist.
  if (chromeos::assistant::features::
          IsProactiveSuggestionsSuppressDuplicatesEnabled()) {
    proactive_suggestions_blacklist_.emplace(
        view_->proactive_suggestions()->hash());
  }

  // The proactive suggestions view will automatically be closed if the user
  // doesn't interact with it within a fixed time interval.
  auto_close_timer_.Start(
      FROM_HERE,
      chromeos::assistant::features::GetProactiveSuggestionsTimeoutThreshold(),
      base::BindRepeating(&AssistantProactiveSuggestionsController::CloseUi,
                          weak_factory_.GetWeakPtr(),
                          ProactiveSuggestionsShowResult::kCloseByTimeout));
}

void AssistantProactiveSuggestionsController::CloseUi(
    ProactiveSuggestionsShowResult result) {
  if (!view_)
    return;

  RecordProactiveSuggestionsShowResult(
      view_->proactive_suggestions()->category(), result);

  auto_close_timer_.Stop();

  view_->GetWidget()->Close();
  view_ = nullptr;
}

}  // namespace ash
