// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/suggestion_container_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

namespace {

// TODO(dmblack): Move common dimensions to shared constant file.
// Appearance.
constexpr int kPaddingDip = 14;
constexpr int kPreferredHeightDip = 48;
constexpr int kSpacingDip = 8;

}  // namespace

SuggestionContainerView::SuggestionContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  // The Assistant controller indirectly owns the view hierarchy to which
  // SuggestionContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size SuggestionContainerView::CalculatePreferredSize() const {
  int width = 0;

  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    gfx::Size child_size = child->GetPreferredSize();

    // Add spacing between chips.
    if (i > 0)
      width += kSpacingDip;

    width += child_size.width();
  }

  // Add horizontal padding.
  if (width > 0)
    width += 2 * kPaddingDip;

  return gfx::Size(width, GetHeightForWidth(width));
}

int SuggestionContainerView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionContainerView::Layout() {
  const int height = views::View::height();
  int left = kPaddingDip;

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    gfx::Size child_size = child->GetPreferredSize();

    child->SetBounds(left, (height - child_size.height()) / 2,
                     child_size.width(), child_size.height());

    left += child_size.width() + kSpacingDip;
  }
}

void SuggestionContainerView::OnSuggestionsAdded(
    const std::map<int, AssistantSuggestion*>& suggestions) {
  for (const std::pair<int, AssistantSuggestion*>& suggestion : suggestions) {
    app_list::SuggestionChipView::Params params;
    params.text = base::UTF8ToUTF16(suggestion.second->text);

    // TODO(dmblack): Here we are using a placeholder image for the suggestion
    // chip icon but we need to handle the actual icon URL.
    if (!suggestion.second->icon_url.is_empty())
      params.icon = gfx::CreateVectorIcon(
          kCircleIcon, app_list::SuggestionChipView::kIconSizeDip,
          gfx::kGoogleGrey300);

    views::View* suggestion_chip_view =
        new app_list::SuggestionChipView(params, /*listener=*/this);

    // When adding a SuggestionChipView, we give the view the same id by which
    // the interaction model identifies the corresponding suggestion. This
    // allows us to look up the suggestion for the view during event handling.
    suggestion_chip_view->set_id(suggestion.first);

    AddChildView(suggestion_chip_view);
  }
  SetVisible(has_children());
}

void SuggestionContainerView::OnSuggestionsCleared() {
  SetVisible(false);
  RemoveAllChildViews(/*delete_children=*/true);
}

void SuggestionContainerView::OnSuggestionChipPressed(
    app_list::SuggestionChipView* suggestion_chip_view) {
  assistant_controller_->OnSuggestionChipPressed(suggestion_chip_view->id());
}

}  // namespace ash
