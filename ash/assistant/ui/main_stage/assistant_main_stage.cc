// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_main_stage.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/suggestion_container_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

namespace {

// StackLayout -----------------------------------------------------------------

// A layout manager which lays out its views atop each other. This differs from
// FillLayout in that we respect the preferred size of views during layout. In
// contrast, FillLayout will cause its views to match the bounds of the host.
class StackLayout : public views::LayoutManager {
 public:
  StackLayout() = default;
  ~StackLayout() override = default;

  gfx::Size GetPreferredSize(const views::View* host) const override {
    gfx::Size preferred_size;

    for (int i = 0; i < host->child_count(); ++i)
      preferred_size.SetToMax(host->child_at(i)->GetPreferredSize());

    return preferred_size;
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    int preferred_height = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      preferred_height = std::max(host->child_at(i)->GetHeightForWidth(width),
                                  preferred_height);
    }

    return preferred_height;
  }

  void Layout(views::View* host) override {
    const int host_width = host->GetContentsBounds().width();

    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);

      int child_width = std::min(child->GetPreferredSize().width(), host_width);
      int child_height = child->GetHeightForWidth(child_width);

      // Children are horizontally centered, top aligned.
      child->SetBounds(/*x=*/(host_width - child_width) / 2, /*y=*/0,
                       child_width, child_height);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StackLayout);
};

}  // namespace

// AssistantMainStage ----------------------------------------------------------

AssistantMainStage::AssistantMainStage(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  InitLayout(assistant_controller);

  // The view hierarchy will be destructed before Shell, which owns
  // AssistantController, so AssistantController is guaranteed to outlive the
  // AssistantMainStage.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

AssistantMainStage::~AssistantMainStage() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

void AssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewBoundsChanged(views::View* view) {
  if (view == committed_query_view_) {
    UpdateCommittedQueryViewSpacer();
  } else if (view == pending_query_view_) {
    // The pending query should be bottom aligned in its parent until it is
    // committed at which point it is animated to the top.
    const int top_offset =
        query_layout_container_->height() - pending_query_view_->height();

    gfx::Transform transform;
    transform.Translate(0, top_offset);
    pending_query_view_->layer()->SetTransform(transform);
  }
}

void AssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewVisibilityChanged(views::View* view) {
  PreferredSizeChanged();
}

void AssistantMainStage::InitLayout(AssistantController* assistant_controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  InitContentLayoutContainer(assistant_controller);
  InitQueryLayoutContainer(assistant_controller);
}

void AssistantMainStage::InitContentLayoutContainer(
    AssistantController* assistant_controller) {
  // Note that we will observe children of |content_layout_container| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |content_layout_container| may not change size in
  // response to these events, necessitating an explicit layout pass.
  views::View* content_layout_container = new views::View();

  views::BoxLayout* layout_manager = content_layout_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Committed query spacer.
  // Note: This view reserves layout space for |committed_query_view_|,
  // dynamically mirroring its preferred size and visibility.
  committed_query_view_spacer_ = new views::View();
  committed_query_view_spacer_->AddObserver(this);
  content_layout_container->AddChildView(committed_query_view_spacer_);

  // UI element container.
  ui_element_container_ = new UiElementContainerView(assistant_controller);
  ui_element_container_->AddObserver(this);
  content_layout_container->AddChildView(ui_element_container_);

  layout_manager->SetFlexForView(ui_element_container_, 1);

  // Suggestion container.
  suggestion_container_ = new SuggestionContainerView(assistant_controller);
  suggestion_container_->AddObserver(this);

  // The suggestion container will be animated on its own layer.
  suggestion_container_->SetPaintToLayer();
  suggestion_container_->layer()->SetFillsBoundsOpaquely(false);

  content_layout_container->AddChildView(suggestion_container_);

  AddChildView(content_layout_container);
}

void AssistantMainStage::InitQueryLayoutContainer(
    AssistantController* assistant_controller) {
  // Note that we will observe children of |query_layout_container_| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |query_layout_container_| may not change size in response
  // to these events, thereby requiring an explicit layout pass.
  query_layout_container_ = new views::View();
  query_layout_container_->set_can_process_events_within_subtree(false);
  query_layout_container_->SetLayoutManager(std::make_unique<StackLayout>());

  AddChildView(query_layout_container_);
}

// TODO(dmblack): Animate transformation.
void AssistantMainStage::OnCommittedQueryChanged(const AssistantQuery& query) {
  // Clean up any previous committed query.
  OnCommittedQueryCleared();

  committed_query_view_ = pending_query_view_;
  pending_query_view_ = nullptr;

  // Update the view and move it to the top of its parent.
  committed_query_view_->SetQuery(query);
  committed_query_view_->layer()->SetTransform(gfx::Transform());

  UpdateCommittedQueryViewSpacer();
  UpdateSuggestionContainer();
}

void AssistantMainStage::OnCommittedQueryCleared() {
  if (!committed_query_view_)
    return;

  query_layout_container_->RemoveChildView(committed_query_view_);

  delete committed_query_view_;
  committed_query_view_ = nullptr;

  UpdateCommittedQueryViewSpacer();
}

void AssistantMainStage::OnPendingQueryChanged(const AssistantQuery& query) {
  if (!pending_query_view_) {
    pending_query_view_ = new AssistantQueryView();
    pending_query_view_->AddObserver(this);

    // The query view will be animated on its own layer.
    pending_query_view_->SetPaintToLayer();
    pending_query_view_->layer()->SetFillsBoundsOpaquely(false);

    query_layout_container_->AddChildView(pending_query_view_);

    UpdateSuggestionContainer();
  }

  pending_query_view_->SetQuery(query);
}

void AssistantMainStage::OnPendingQueryCleared() {
  if (pending_query_view_) {
    query_layout_container_->RemoveChildView(pending_query_view_);

    delete pending_query_view_;
    pending_query_view_ = nullptr;
  }

  UpdateSuggestionContainer();
}

void AssistantMainStage::UpdateCommittedQueryViewSpacer() {
  // The spacer reserves room in the layout for the committed query view, so
  // it should match its size.
  committed_query_view_spacer_->SetPreferredSize(
      committed_query_view_ ? committed_query_view_->size() : gfx::Size());
}

// TODO(dmblack): Animate visibility changes.
void AssistantMainStage::UpdateSuggestionContainer() {
  // The suggestion container is only visible when the pending query is not.
  // When it is not visible, it should not process events.
  bool visible = pending_query_view_ == nullptr;
  suggestion_container_->layer()->SetOpacity(visible ? 1.f : 0.f);
  suggestion_container_->set_can_process_events_within_subtree(visible ? true
                                                                       : false);
}

}  // namespace ash
