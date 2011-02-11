// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/base_tab_strip.h"

#include "base/logging.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/dragged_tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "views/widget/native_widget_win.h"
#endif

namespace {

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit ResetDraggingStateDelegate(BaseTab* tab) : tab_(tab) {
  }

  virtual void AnimationEnded(const ui::Animation* animation) {
    tab_->set_dragging(false);
  }

  virtual void AnimationCanceled(const ui::Animation* animation) {
    tab_->set_dragging(false);
  }

 private:
  BaseTab* tab_;

  DISALLOW_COPY_AND_ASSIGN(ResetDraggingStateDelegate);
};

}  // namespace

// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class BaseTabStrip::RemoveTabDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  RemoveTabDelegate(BaseTabStrip* tab_strip, BaseTab* tab)
      : tabstrip_(tab_strip),
        tab_(tab) {
  }

  virtual void AnimationEnded(const ui::Animation* animation) {
    CompleteRemove();
  }

  virtual void AnimationCanceled(const ui::Animation* animation) {
    // We can be canceled for two interesting reasons:
    // . The tab we reference was dragged back into the tab strip. In this case
    //   we don't want to remove the tab (closing is false).
    // . The drag was completed before the animation completed
    //   (DestroyDraggedSourceTab). In this case we need to remove the tab
    //   (closing is true).
    if (tab_->closing())
      CompleteRemove();
  }

 private:
  void CompleteRemove() {
    if (!tab_->closing()) {
      // The tab was added back yet we weren't canceled. This shouldn't happen.
      NOTREACHED();
      return;
    }
    tabstrip_->RemoveAndDeleteTab(tab_);
    HighlightCloseButton();
  }

  // When the animation completes, we send the Container a message to simulate
  // a mouse moved event at the current mouse position. This tickles the Tab
  // the mouse is currently over to show the "hot" state of the close button.
  void HighlightCloseButton() {
    if (tabstrip_->IsDragSessionActive() ||
        !tabstrip_->ShouldHighlightCloseButtonAfterRemove()) {
      // This function is not required (and indeed may crash!) for removes
      // spawned by non-mouse closes and drag-detaches.
      return;
    }

#if defined(OS_WIN)
    views::Widget* widget = tabstrip_->GetWidget();
    // This can be null during shutdown. See http://crbug.com/42737.
    if (!widget)
      return;
    // Force the close button (that slides under the mouse) to highlight by
    // saying the mouse just moved, but sending the same coordinates.
    DWORD pos = GetMessagePos();
    POINT cursor_point = {GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
    MapWindowPoints(NULL, widget->GetNativeView(), &cursor_point, 1);

    static_cast<views::NativeWidgetWin*>(widget)->ResetLastMouseMoveFlag();
    // Return to message loop - otherwise we may disrupt some operation that's
    // in progress.
    SendMessage(widget->GetNativeView(), WM_MOUSEMOVE, 0,
                MAKELPARAM(cursor_point.x, cursor_point.y));
#else
    NOTIMPLEMENTED();
#endif
  }

  BaseTabStrip* tabstrip_;
  BaseTab* tab_;

  DISALLOW_COPY_AND_ASSIGN(RemoveTabDelegate);
};

BaseTabStrip::BaseTabStrip(TabStripController* controller, Type type)
    : controller_(controller),
      type_(type),
      attaching_dragged_tab_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(bounds_animator_(this)) {
}

BaseTabStrip::~BaseTabStrip() {
}

void BaseTabStrip::UpdateLoadingAnimations() {
  controller_->UpdateLoadingAnimations();
}

bool BaseTabStrip::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

BaseTab* BaseTabStrip::GetSelectedBaseTab() const {
  return GetBaseTabAtModelIndex(controller_->GetSelectedIndex());
}

void BaseTabStrip::AddTabAt(int model_index,
                            bool foreground,
                            const TabRendererData& data) {
  BaseTab* tab = CreateTab();
  tab->SetData(data);

  TabData d = { tab, gfx::Rect() };
  tab_data_.insert(tab_data_.begin() + ModelIndexToTabIndex(model_index), d);

  AddChildView(tab);

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (tab_count() > 1 && GetWindow() && GetWindow()->IsVisible())
    StartInsertTabAnimation(model_index, foreground);
  else
    DoLayout();
}

void BaseTabStrip::MoveTab(int from_model_index, int to_model_index) {
  int from_tab_data_index = ModelIndexToTabIndex(from_model_index);

  BaseTab* tab = tab_data_[from_tab_data_index].tab;
  tab_data_.erase(tab_data_.begin() + from_tab_data_index);

  TabData data = {tab, gfx::Rect()};

  int to_tab_data_index = ModelIndexToTabIndex(to_model_index);

  tab_data_.insert(tab_data_.begin() + to_tab_data_index, data);

  StartMoveTabAnimation();
}

void BaseTabStrip::SetTabData(int model_index, const TabRendererData& data) {
  BaseTab* tab = GetBaseTabAtModelIndex(model_index);
  bool mini_state_changed = tab->data().mini != data.mini;
  tab->SetData(data);
  tab->SchedulePaint();

  if (mini_state_changed) {
    if (GetWindow() && GetWindow()->IsVisible())
      StartMiniTabAnimation();
    else
      DoLayout();
  }
}

BaseTab* BaseTabStrip::GetBaseTabAtModelIndex(int model_index) const {
  return base_tab_at_tab_index(ModelIndexToTabIndex(model_index));
}

int BaseTabStrip::GetModelIndexOfBaseTab(const BaseTab* tab) const {
  for (int i = 0, model_index = 0; i < tab_count(); ++i) {
    BaseTab* current_tab = base_tab_at_tab_index(i);
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

bool BaseTabStrip::IsActiveDropTarget() const {
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (tab->dragging())
      return true;
  }
  return false;
}

void BaseTabStrip::SelectTab(BaseTab* tab) {
  int model_index = GetModelIndexOfBaseTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->SelectTab(model_index);
}

void BaseTabStrip::CloseTab(BaseTab* tab) {
  // Find the closest model index. We do this so that the user can rapdily close
  // tabs and have the close click close the next tab.
  int model_index = 0;
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* current_tab = base_tab_at_tab_index(i);
    if (current_tab == tab)
      break;
    if (!current_tab->closing())
      model_index++;
  }

  if (IsValidModelIndex(model_index))
    controller_->CloseTab(model_index);
}

void BaseTabStrip::ShowContextMenu(BaseTab* tab, const gfx::Point& p) {
  controller_->ShowContextMenu(tab, p);
}

bool BaseTabStrip::IsTabSelected(const BaseTab* tab) const {
  int model_index = GetModelIndexOfBaseTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabSelected(model_index);
}

bool BaseTabStrip::IsTabPinned(const BaseTab* tab) const {
  if (tab->closing())
    return false;

  int model_index = GetModelIndexOfBaseTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabPinned(model_index);
}

bool BaseTabStrip::IsTabCloseable(const BaseTab* tab) const {
  int model_index = GetModelIndexOfBaseTab(tab);
  return !IsValidModelIndex(model_index) ||
      controller_->IsTabCloseable(model_index);
}

void BaseTabStrip::MaybeStartDrag(BaseTab* tab,
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

BaseTab* BaseTabStrip::GetTabAt(BaseTab* tab,
                                const gfx::Point& tab_in_tab_coordinates) {
  gfx::Point local_point = tab_in_tab_coordinates;
  ConvertPointToView(tab, this, &local_point);
  views::View* view = GetViewForPoint(local_point);
  if (!view)
    return NULL;  // No tab contains the point.

  // Walk up the view hierarchy until we find a tab, or the TabStrip.
  while (view && view != this && view->GetID() != VIEW_ID_TAB)
    view = view->parent();

  return view && view->GetID() == VIEW_ID_TAB ?
      static_cast<BaseTab*>(view) : NULL;
}

void BaseTabStrip::Layout() {
  // Only do a layout if our size changed.
  if (last_layout_size_ == size())
    return;
  DoLayout();
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

void BaseTabStrip::StartMoveTabAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void BaseTabStrip::StartRemoveTabAnimation(int model_index) {
  PrepareForAnimation();

  // Mark the tab as closing.
  BaseTab* tab = GetBaseTabAtModelIndex(model_index);
  tab->set_closing(true);

  // Start an animation for the tabs.
  GenerateIdealBounds();
  AnimateToIdealBounds();

  // Animate the tab being closed to 0x0.
  gfx::Rect tab_bounds = tab->bounds();
  if (type() == HORIZONTAL_TAB_STRIP)
    tab_bounds.set_width(0);
  else
    tab_bounds.set_height(0);
  bounds_animator_.AnimateViewTo(tab, tab_bounds);

  // Register delegate to do cleanup when done, BoundsAnimator takes
  // ownership of RemoveTabDelegate.
  bounds_animator_.SetAnimationDelegate(tab, new RemoveTabDelegate(this, tab),
                                        true);
}

void BaseTabStrip::StartMiniTabAnimation() {
  PrepareForAnimation();

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void BaseTabStrip::RemoveAndDeleteTab(BaseTab* tab) {
  int tab_data_index = TabIndexOfTab(tab);

  DCHECK(tab_data_index != -1);

  // Remove the Tab from the TabStrip's list...
  tab_data_.erase(tab_data_.begin() + tab_data_index);

  delete tab;
}

int BaseTabStrip::TabIndexOfTab(BaseTab* tab) const {
  for (int i = 0; i < tab_count(); ++i) {
    if (base_tab_at_tab_index(i) == tab)
      return i;
  }
  return -1;
}

void BaseTabStrip::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  bounds_animator().Cancel();

  if (layout)
    DoLayout();
}

void BaseTabStrip::DestroyDragController() {
  if (IsDragSessionActive())
    drag_controller_.reset(NULL);
}

void BaseTabStrip::StartedDraggingTab(BaseTab* tab) {
  PrepareForAnimation();

  // Reset the dragging state of all other tabs. We do this as the painting code
  // only handles one tab being dragged at a time. If another tab is marked as
  // dragging, it should also be closing.
  for (int i = 0; i < tab_count(); ++i)
    base_tab_at_tab_index(i)->set_dragging(false);

  tab->set_dragging(true);

  // Stop any animations on the tab.
  bounds_animator_.StopAnimatingView(tab);

  // Move the tab to its ideal bounds.
  GenerateIdealBounds();
  int tab_data_index = TabIndexOfTab(tab);
  DCHECK(tab_data_index != -1);
  tab->SetBoundsRect(ideal_bounds(tab_data_index));
  SchedulePaint();
}

void BaseTabStrip::StoppedDraggingTab(BaseTab* tab) {
  int tab_data_index = TabIndexOfTab(tab);
  if (tab_data_index == -1) {
    // The tab was removed before the drag completed. Don't do anything.
    return;
  }

  PrepareForAnimation();

  // Animate the view back to its correct position.
  GenerateIdealBounds();
  AnimateToIdealBounds();
  bounds_animator_.AnimateViewTo(tab, ideal_bounds(TabIndexOfTab(tab)));

  // Install a delegate to reset the dragging state when done. We have to leave
  // dragging true for the tab otherwise it'll draw beneath the new tab button.
  bounds_animator_.SetAnimationDelegate(tab,
                                        new ResetDraggingStateDelegate(tab),
                                        true);
}

void BaseTabStrip::PrepareForAnimation() {
  if (!IsDragSessionActive() && !DraggedTabController::IsAttachedTo(this)) {
    for (int i = 0; i < tab_count(); ++i)
      base_tab_at_tab_index(i)->set_dragging(false);
  }
}

ui::AnimationDelegate* BaseTabStrip::CreateRemoveTabDelegate(BaseTab* tab) {
  return new RemoveTabDelegate(this, tab);
}

void BaseTabStrip::DoLayout() {
  last_layout_size_ = size();

  StopAnimating(false);

  GenerateIdealBounds();

  for (int i = 0; i < tab_count(); ++i)
    tab_data_[i].tab->SetBoundsRect(tab_data_[i].ideal_bounds);

  SchedulePaint();
}
