// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/base_tab_strip.h"

#include "base/logging.h"
#include "chrome/browser/views/tabs/dragged_tab_controller.h"
#include "chrome/browser/views/tabs/tab_strip_controller.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

BaseTabStrip::BaseTabStrip(TabStripController* controller, Type type)
    : controller_(controller),
      type_(type),
      attaching_dragged_tab_(false) {
}

BaseTabStrip::~BaseTabStrip() {
}

void BaseTabStrip::UpdateLoadingAnimations() {
  controller_->UpdateLoadingAnimations();
}

BaseTabRenderer* BaseTabStrip::GetSelectedBaseTab() const {
  return GetBaseTabAtModelIndex(controller_->GetSelectedIndex());
}

void BaseTabStrip::AddTabAt(int model_index,
                            bool foreground,
                            const TabRendererData& data) {
  BaseTabRenderer* tab = CreateTab();
  tab->SetData(data);

  TabData d = { tab, gfx::Rect() };
  tab_data_.insert(tab_data_.begin() + ModelIndexToTabIndex(model_index), d);

  AddChildView(tab);

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (tab_count() > 1 && GetWindow() && GetWindow()->IsVisible())
    StartInsertTabAnimation(model_index, foreground);
  else
    Layout();
}

void BaseTabStrip::MoveTab(int from_model_index, int to_model_index) {
  int from_tab_data_index = ModelIndexToTabIndex(from_model_index);

  BaseTabRenderer* tab = tab_data_[from_tab_data_index].tab;
  tab_data_.erase(tab_data_.begin() + from_tab_data_index);

  TabData data = {tab, gfx::Rect()};

  int to_tab_data_index = ModelIndexToTabIndex(to_model_index);

  tab_data_.insert(tab_data_.begin() + to_tab_data_index, data);

  StartMoveTabAnimation();
}

BaseTabRenderer* BaseTabStrip::GetBaseTabAtModelIndex(int model_index) const {
  return base_tab_at_tab_index(ModelIndexToTabIndex(model_index));
}

int BaseTabStrip::GetModelIndexOfBaseTab(const BaseTabRenderer* tab) const {
  for (int i = 0, model_index = 0; i < tab_count(); ++i) {
    BaseTabRenderer* current_tab = base_tab_at_tab_index(i);
    if (!current_tab->closing()) {
      if (current_tab == tab)
        return model_index;
      model_index++;
    }
  }
  return -1;
}

int BaseTabStrip::GetModelCount() const {
  return controller_->GetCount();
}

bool BaseTabStrip::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

int BaseTabStrip::ModelIndexToTabIndex(int model_index) const {
  int current_model_index = 0;
  for (int i = 0; i < tab_count(); ++i) {
    if (!base_tab_at_tab_index(i)->closing()) {
      if (current_model_index == model_index)
        return i;
      current_model_index++;
    }
  }
  return static_cast<int>(tab_data_.size());
}

bool BaseTabStrip::IsDragSessionActive() const {
  return drag_controller_.get() != NULL;
}

void BaseTabStrip::SelectTab(BaseTabRenderer* tab) {
  int model_index = GetModelIndexOfBaseTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->SelectTab(model_index);
}

void BaseTabStrip::CloseTab(BaseTabRenderer* tab) {
  int model_index = GetModelIndexOfBaseTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->CloseTab(model_index);
}

void BaseTabStrip::ShowContextMenu(BaseTabRenderer* tab, const gfx::Point& p) {
  controller_->ShowContextMenu(tab, p);
}

bool BaseTabStrip::IsTabSelected(const BaseTabRenderer* tab) const {
  int model_index = GetModelIndexOfBaseTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabSelected(model_index);
}

bool BaseTabStrip::IsTabPinned(const BaseTabRenderer* tab) const {
  if (tab->closing())
    return false;

  int model_index = GetModelIndexOfBaseTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabPinned(model_index);
}

void BaseTabStrip::MaybeStartDrag(BaseTabRenderer* tab,
                                  const views::MouseEvent& event) {
  // Don't accidentally start any drag operations during animations if the
  // mouse is down... during an animation tabs are being resized automatically,
  // so the View system can misinterpret this easily if the mouse is down that
  // the user is dragging.
  if (IsAnimating() || tab->closing() ||
      controller_->HasAvailableDragActions() == 0) {
    return;
  }
  int model_index = GetModelIndexOfBaseTab(tab);
  if (!IsValidModelIndex(model_index)) {
    CHECK(false);
    return;
  }
  drag_controller_.reset(new DraggedTabController(tab, this));
  drag_controller_->CaptureDragInfo(tab, event.location());
}

void BaseTabStrip::ContinueDrag(const views::MouseEvent& event) {
  // We can get called even if |MaybeStartDrag| wasn't called in the event of
  // a TabStrip animation when the mouse button is down. In this case we should
  // _not_ continue the drag because it can lead to weird bugs.
  if (drag_controller_.get()) {
    bool started_drag = drag_controller_->started_drag();
    drag_controller_->Drag();
    if (drag_controller_->started_drag() && !started_drag) {
      // The drag just started. Redirect mouse events to us to that the tab that
      // originated the drag can be safely deleted.
      GetRootView()->SetMouseHandler(this);
    }
  }
}

bool BaseTabStrip::EndDrag(bool canceled) {
  if (!drag_controller_.get())
    return false;
  bool started_drag = drag_controller_->started_drag();
  drag_controller_->EndDrag(canceled);
  return started_drag;
}

void BaseTabStrip::Layout() {
  StopAnimating(false);

  GenerateIdealBounds();

  for (int i = 0; i < tab_count(); ++i)
    tab_data_[i].tab->SetBounds(tab_data_[i].ideal_bounds);

  SchedulePaint();
}

bool BaseTabStrip::OnMouseDragged(const views::MouseEvent&  event) {
  if (drag_controller_.get())
    drag_controller_->Drag();
  return true;
}

void BaseTabStrip::OnMouseReleased(const views::MouseEvent& event,
                                   bool canceled) {
  EndDrag(canceled);
}

void BaseTabStrip::RemoveAndDeleteTab(BaseTabRenderer* tab) {
  int tab_data_index = TabIndexOfTab(tab);

  DCHECK(tab_data_index != -1);

  // Remove the Tab from the TabStrip's list...
  tab_data_.erase(tab_data_.begin() + tab_data_index);

  delete tab;
}

int BaseTabStrip::TabIndexOfTab(BaseTabRenderer* tab) const {
  for (int i = 0; i < tab_count(); ++i) {
    if (base_tab_at_tab_index(i) == tab)
      return i;
  }
  return -1;
}

void BaseTabStrip::DestroyDragController() {
  if (IsDragSessionActive())
    drag_controller_.reset(NULL);
}
