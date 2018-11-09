// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_response_processor.h"

#include <algorithm>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/base64.h"
#include "base/stl_util.h"

namespace ash {

namespace {

// WebContents.
constexpr char kDataUriPrefix[] = "data:text/html;base64,";

}  // namespace

// AssistantCardProcessor ------------------------------------------------------

AssistantCardProcessor::AssistantCardProcessor(
    AssistantController* assistant_controller,
    AssistantResponseProcessor* assistant_response_processor,
    AssistantCardElement* assistant_card_element)
    : assistant_controller_(assistant_controller),
      assistant_response_processor_(assistant_response_processor),
      assistant_card_element_(assistant_card_element) {}

AssistantCardProcessor::~AssistantCardProcessor() {
  if (contents_)
    contents_->RemoveObserver(this);
}

void AssistantCardProcessor::DidStopLoading() {
  contents_->RemoveObserver(this);

  // Transfer ownership of |contents_| to the card element and notify the
  // response processor that we've finished processing.
  assistant_card_element_->set_contents(std::move(contents_));
  assistant_response_processor_->DidFinishProcessing(this);
}

void AssistantCardProcessor::Process() {
  assistant_controller_->GetNavigableContentsFactory(
      mojo::MakeRequest(&contents_factory_));

  // TODO(dmblack): Find a better way of determining desired card size.
  const int width_dip = kPreferredWidthDip - 2 * kUiElementHorizontalMarginDip;

  // Configure parameters for the card.
  auto contents_params = content::mojom::NavigableContentsParams::New();
  contents_params->enable_view_auto_resize = true;
  contents_params->auto_resize_min_size = gfx::Size(width_dip, 1);
  contents_params->auto_resize_max_size = gfx::Size(width_dip, INT_MAX);
  contents_params->suppress_navigations = true;

  contents_ = std::make_unique<content::NavigableContents>(
      contents_factory_.get(), std::move(contents_params));

  // Observe |contents_| so that we are notified when loading is complete.
  contents_->AddObserver(this);

  // Navigate to the data URL which represents the card.
  std::string encoded_html;
  base::Base64Encode(assistant_card_element_->html(), &encoded_html);
  contents_->Navigate(GURL(kDataUriPrefix + encoded_html));
}

// AssistantResponseProcessor::Task --------------------------------------------

AssistantResponseProcessor::Task::Task(AssistantResponse& response,
                                       ProcessCallback callback)
    : response(response.GetWeakPtr()), callback(std::move(callback)) {}

AssistantResponseProcessor::Task::~Task() = default;

// AssistantResponseProcessor --------------------------------------------------

AssistantResponseProcessor::AssistantResponseProcessor(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), weak_factory_(this) {}

AssistantResponseProcessor::~AssistantResponseProcessor() = default;

void AssistantResponseProcessor::Process(AssistantResponse& response,
                                         ProcessCallback callback) {
  // We should only attempt to process responses that are unprocessed.
  DCHECK_EQ(AssistantResponse::ProcessingState::kUnprocessed,
            response.processing_state());

  // Update processing state.
  response.set_processing_state(
      AssistantResponse::ProcessingState::kProcessing);

  // We only support processing a single task at a time. As such, we should
  // abort any task in progress before creating and starting a new one.
  TryAbortingTask();

  // Create a task.
  task_.emplace(/*response=*/response,
                /*callback=*/std::move(callback));

  // Start processing UI elements.
  for (const auto& ui_element : response.GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        // Create and start an element processor to process the card element.
        task_->element_processors.push_back(
            std::make_unique<AssistantCardProcessor>(
                assistant_controller_, this,
                static_cast<AssistantCardElement*>(ui_element.get())));
        task_->element_processors.back()->Process();
        break;
      case AssistantUiElementType::kText:
        // No processing necessary.
        break;
    }
  }

  // Try finishing. This will no-op if there are still UI elements being
  // processed asynchronously.
  TryFinishingTask();
}

void AssistantResponseProcessor::DidFinishProcessing(
    const AssistantCardProcessor* card_processor) {
  // If the response has been invalidated we should abort early.
  if (!task_->response) {
    TryAbortingTask();
    return;
  }

  // Remove the finished element processor to indicate its completion.
  base::EraseIf(task_->element_processors,
                [card_processor](
                    const std::unique_ptr<AssistantCardProcessor>& candidate) {
                  return candidate.get() == card_processor;
                });

  // Try finishing. This will no-op if there are still UI elements being
  // processed asynchronously.
  TryFinishingTask();
}

void AssistantResponseProcessor::TryAbortingTask() {
  if (!task_)
    return;

  // Invalidate weak pointers to prevent processing callbacks from running.
  // Otherwise we might continue receiving card events for the aborted task.
  weak_factory_.InvalidateWeakPtrs();

  // Notify our callback and clean up any task related resources.
  std::move(task_->callback).Run(/*success=*/false);
  task_.reset();
}

void AssistantResponseProcessor::TryFinishingTask() {
  // This method is a no-op if we are still processing.
  if (!task_->element_processors.empty())
    return;

  // Update processing state.
  task_->response->set_processing_state(
      AssistantResponse::ProcessingState::kProcessed);

  // Notify our callback and clean up any task related resources.
  std::move(task_->callback).Run(/*success=*/true);
  task_.reset();
}

}  // namespace ash
