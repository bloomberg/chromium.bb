// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_footer_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/assistant/ui/main_stage/suggestion_container_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 48;

}  // namespace

AssistantFooterView::AssistantFooterView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  InitLayout();
}

AssistantFooterView::~AssistantFooterView() = default;

gfx::Size AssistantFooterView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantFooterView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantFooterView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantFooterView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

// TODO(dmblack): Handle opted out/in state.
void AssistantFooterView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Suggestion container.
  suggestion_container_ = new SuggestionContainerView(assistant_controller_);
  AddChildView(suggestion_container_);

  // Opt in view.
  opt_in_view_ = new AssistantOptInView();
  opt_in_view_->SetVisible(false);
  AddChildView(opt_in_view_);
}

}  // namespace ash
