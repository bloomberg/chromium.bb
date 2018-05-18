// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/suggestion_container_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// TODO(dmblack): Move common dimensions to shared constant file.
// Appearance.
constexpr int kPaddingDip = 14;
constexpr int kPreferredHeightDip = 48;
constexpr int kSpacingDip = 8;

// InvisibleScrollBar ----------------------------------------------------------

class InvisibleScrollBar : public views::OverlayScrollBar {
 public:
  explicit InvisibleScrollBar(bool horizontal)
      : views::OverlayScrollBar(horizontal) {}

  ~InvisibleScrollBar() override = default;

  // views::OverlayScrollBar:
  int GetThickness() const override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(InvisibleScrollBar);
};

}  // namespace

// SuggestionContainerView -----------------------------------------------------

SuggestionContainerView::SuggestionContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      contents_view_(new views::View()) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // SuggestionContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size SuggestionContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int SuggestionContainerView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionContainerView::InitLayout() {
  // Contents.
  views::BoxLayout* layout_manager =
      contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // ScrollView.
  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetContents(contents_view_);
  SetHorizontalScrollBar(new InvisibleScrollBar(/*horizontal=*/true));
  SetVerticalScrollBar(new InvisibleScrollBar(/*horizontal=*/false));
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

    contents_view_->AddChildView(suggestion_chip_view);
  }
  UpdateContentsBounds();
  SetVisible(contents_view_->has_children());
}

void SuggestionContainerView::OnSuggestionsCleared() {
  SetVisible(false);
  contents_view_->RemoveAllChildViews(/*delete_children=*/true);
  UpdateContentsBounds();
}

void SuggestionContainerView::OnSuggestionChipPressed(
    app_list::SuggestionChipView* suggestion_chip_view) {
  assistant_controller_->OnSuggestionChipPressed(suggestion_chip_view->id());
}

void SuggestionContainerView::UpdateContentsBounds() {
  contents_view_->SetBounds(0, 0, contents_view_->GetPreferredSize().width(),
                            kPreferredHeightDip);
}

}  // namespace ash
