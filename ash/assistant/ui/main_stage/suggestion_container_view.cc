// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_container_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 48;

}  // namespace

// SuggestionContainerView -----------------------------------------------------

SuggestionContainerView::SuggestionContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      download_request_weak_factory_(this) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // SuggestionContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size SuggestionContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int SuggestionContainerView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_width = content_view->GetPreferredSize().width();
  content_view->SetSize(gfx::Size(preferred_width, kPreferredHeightDip));
}

void SuggestionContainerView::InitLayout() {
  views::BoxLayout* layout_manager =
      content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_END);
}

void SuggestionContainerView::OnResponseChanged(
    const AssistantResponse& response) {
  OnResponseCleared();

  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  for (const std::pair<int, const AssistantSuggestion*>& suggestion :
       response.GetSuggestions()) {
    // We will use the same identifier by which the Assistant interaction model
    // uniquely identifies a suggestion to uniquely identify its corresponding
    // suggestion chip view.
    const int id = suggestion.first;

    app_list::SuggestionChipView::Params params;
    params.assistant_style = true;
    params.text = base::UTF8ToUTF16(suggestion.second->text);

    if (!suggestion.second->icon_url.is_empty()) {
      // Initiate a request to download the image for the suggestion chip icon.
      // Note that the request is identified by the suggestion id.
      assistant_controller_->DownloadImage(
          suggestion.second->icon_url,
          base::BindOnce(
              &SuggestionContainerView::OnSuggestionChipIconDownloaded,
              download_request_weak_factory_.GetWeakPtr(), id));

      // To reserve layout space until the actual icon can be downloaded, we
      // supply an empty placeholder image to the suggestion chip view.
      params.icon = gfx::ImageSkia();
    }

    app_list::SuggestionChipView* suggestion_chip_view =
        new app_list::SuggestionChipView(params, /*listener=*/this);

    // Given a suggestion chip view, we need to be able to look up the id of
    // the underlying suggestion. This is used for handling press events.
    suggestion_chip_view->set_id(id);

    // Given an id, we also want to be able to look up the corresponding
    // suggestion chip view. This is used for handling icon download events.
    suggestion_chip_views_[id] = suggestion_chip_view;

    content_view()->AddChildView(suggestion_chip_view);
  }
}

void SuggestionContainerView::OnResponseCleared() {
  // Abort any download requests in progress.
  download_request_weak_factory_.InvalidateWeakPtrs();

  // When modifying the view hierarchy, make sure we keep our view cache synced.
  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  suggestion_chip_views_.clear();
}

void SuggestionContainerView::OnSuggestionChipIconDownloaded(
    int id,
    const gfx::ImageSkia& icon) {
  if (!icon.isNull())
    suggestion_chip_views_[id]->SetIcon(icon);
}

void SuggestionContainerView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  assistant_controller_->interaction_controller()->OnSuggestionChipPressed(
      static_cast<app_list::SuggestionChipView*>(sender)->id());
}

}  // namespace ash
