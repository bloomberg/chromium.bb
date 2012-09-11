// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include <algorithm>

#include "ash/launcher/app_list_button.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_tooltip_manager.h"
#include "ash/launcher/overflow_bubble.h"
#include "ash/launcher/overflow_button.h"
#include "ash/launcher/tabbed_launcher_button.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/auto_reset.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

using ui::Animation;
using views::View;

namespace ash {
namespace internal {

// Default amount content is inset on the left edge.
const int kDefaultLeadingInset = 8;

// Minimum distance before drag starts.
const int kMinimumDragDistance = 8;

// Size between the buttons.
const int kButtonSpacing = 4;

namespace {

// Custom FocusSearch used to navigate the launcher in the order items are in
// the ViewModel.
class LauncherFocusSearch : public views::FocusSearch {
 public:
  explicit LauncherFocusSearch(views::ViewModel* view_model)
      : FocusSearch(NULL, true, true),
        view_model_(view_model) {}
  virtual ~LauncherFocusSearch() {}

  // views::FocusSearch overrides:
  virtual View* FindNextFocusableView(
      View* starting_view,
      bool reverse,
      Direction direction,
      bool check_starting_view,
      views::FocusTraversable** focus_traversable,
      View** focus_traversable_view) OVERRIDE {
    int index = view_model_->GetIndexOfView(starting_view);
    if (index == -1)
      return view_model_->view_at(0);

    if (reverse) {
      --index;
      if (index < 0)
        index = view_model_->view_size() - 1;
    } else {
      ++index;
      if (index >= view_model_->view_size())
        index = 0;
    }
    return view_model_->view_at(index);
  }

 private:
  views::ViewModel* view_model_;

  DISALLOW_COPY_AND_ASSIGN(LauncherFocusSearch);
};

// ui::SimpleMenuModel::Delegate implementation that remembers the id of the
// menu that was activated.
class MenuDelegateImpl : public ui::SimpleMenuModel::Delegate {
 public:
  MenuDelegateImpl() : activated_command_id_(-1) {}

  int activated_command_id() const { return activated_command_id_; }

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }
  virtual void ExecuteCommand(int command_id) OVERRIDE {
    activated_command_id_ = command_id;
  }

 private:
  // ID of the command passed to ExecuteCommand.
  int activated_command_id_;

  DISALLOW_COPY_AND_ASSIGN(MenuDelegateImpl);
};

// AnimationDelegate that deletes a view when done. This is used when a launcher
// item is removed, which triggers a remove animation. When the animation is
// done we delete the view.
class DeleteViewAnimationDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit DeleteViewAnimationDelegate(views::View* view) : view_(view) {}
  virtual ~DeleteViewAnimationDelegate() {}

 private:
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(DeleteViewAnimationDelegate);
};

// AnimationDelegate used when inserting a new item. This steadily increases the
// opacity of the layer as the animation progress.
class FadeInAnimationDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit FadeInAnimationDelegate(views::View* view) : view_(view) {}
  virtual ~FadeInAnimationDelegate() {}

  // AnimationDelegate overrides:
  virtual void AnimationProgressed(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  virtual void AnimationEnded(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }
  virtual void AnimationCanceled(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(FadeInAnimationDelegate);
};

void ReflectItemStatus(const ash::LauncherItem& item,
                       LauncherButton* button) {
  switch (item.status) {
    case STATUS_CLOSED:
      button->ClearState(LauncherButton::STATE_ACTIVE);
      button->ClearState(LauncherButton::STATE_RUNNING);
      button->ClearState(LauncherButton::STATE_ATTENTION);
      break;
    case STATUS_RUNNING:
      button->ClearState(LauncherButton::STATE_ACTIVE);
      button->AddState(LauncherButton::STATE_RUNNING);
      button->ClearState(LauncherButton::STATE_ATTENTION);
      break;
    case STATUS_ACTIVE:
      button->AddState(LauncherButton::STATE_ACTIVE);
      button->ClearState(LauncherButton::STATE_RUNNING);
      button->ClearState(LauncherButton::STATE_ATTENTION);
      break;
    case STATUS_ATTENTION:
      button->ClearState(LauncherButton::STATE_ACTIVE);
      button->ClearState(LauncherButton::STATE_RUNNING);
      button->AddState(LauncherButton::STATE_ATTENTION);
      break;
  }
}

}  // namespace

// AnimationDelegate used when inserting a new item. This steadily decreased the
// opacity of the layer as the animation progress.
class LauncherView::FadeOutAnimationDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  FadeOutAnimationDelegate(LauncherView* host, views::View* view)
      : launcher_view_(host),
        view_(view) {}
  virtual ~FadeOutAnimationDelegate() {}

  // AnimationDelegate overrides:
  virtual void AnimationProgressed(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  virtual void AnimationEnded(const Animation* animation) OVERRIDE {
    launcher_view_->AnimateToIdealBounds();
  }
  virtual void AnimationCanceled(const Animation* animation) OVERRIDE {
  }

 private:
  LauncherView* launcher_view_;
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(FadeOutAnimationDelegate);
};

// AnimationDelegate used to trigger fading an element in. When an item is
// inserted this delegate is attached to the animation that expands the size of
// the item.  When done it kicks off another animation to fade the item in.
class LauncherView::StartFadeAnimationDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  StartFadeAnimationDelegate(LauncherView* host,
                             views::View* view)
      : launcher_view_(host),
        view_(view) {}
  virtual ~StartFadeAnimationDelegate() {}

  // AnimationDelegate overrides:
  virtual void AnimationEnded(const Animation* animation) OVERRIDE {
    launcher_view_->FadeIn(view_);
  }
  virtual void AnimationCanceled(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
  }

 private:
  LauncherView* launcher_view_;
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(StartFadeAnimationDelegate);
};

LauncherView::LauncherView(LauncherModel* model,
                           LauncherDelegate* delegate,
                           ShelfLayoutManager* shelf_layout_manager)
    : model_(model),
      delegate_(delegate),
      view_model_(new views::ViewModel),
      first_visible_index_(0),
      last_visible_index_(-1),
      overflow_button_(NULL),
      drag_pointer_(NONE),
      drag_view_(NULL),
      drag_offset_(0),
      start_drag_index_(-1),
      context_menu_id_(0),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      leading_inset_(kDefaultLeadingInset) {
  DCHECK(model_);
  bounds_animator_.reset(new views::BoundsAnimator(this));
  bounds_animator_->AddObserver(this);
  set_context_menu_controller(this);
  focus_search_.reset(new LauncherFocusSearch(view_model_.get()));
  tooltip_.reset(new LauncherTooltipManager(
      alignment_, shelf_layout_manager, this));
}

LauncherView::~LauncherView() {
  bounds_animator_->RemoveObserver(this);
  model_->RemoveObserver(this);
}

void LauncherView::Init() {
  model_->AddObserver(this);

  const LauncherItems& items(model_->items());
  for (LauncherItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    child->SetPaintToLayer(true);
    view_model_->Add(child, static_cast<int>(i - items.begin()));
    AddChildView(child);
  }
  UpdateFirstButtonPadding();
  LauncherStatusChanged();

  overflow_button_ = new OverflowButton(this);
  overflow_button_->set_context_menu_controller(this);
  ConfigureChildView(overflow_button_);
  AddChildView(overflow_button_);

  // We'll layout when our bounds change.
}

void LauncherView::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;
  alignment_ = alignment;
  UpdateFirstButtonPadding();
  overflow_button_->SetShelfAlignment(alignment_);
  LayoutToIdealBounds();
  tooltip_->SetArrowLocation(alignment_);
  if (overflow_bubble_.get())
    overflow_bubble_->Hide();
}

gfx::Rect LauncherView::GetIdealBoundsOfItemIcon(LauncherID id) {
  int index = model_->ItemIndexByID(id);
  if (index == -1 || index > last_visible_index_)
    return gfx::Rect();
  const gfx::Rect& ideal_bounds(view_model_->ideal_bounds(index));
  DCHECK_NE(TYPE_APP_LIST, model_->items()[index].type);
  LauncherButton* button =
      static_cast<LauncherButton*>(view_model_->view_at(index));
  gfx::Rect icon_bounds = button->GetIconBounds();
  return gfx::Rect(ideal_bounds.x() + icon_bounds.x(),
                   ideal_bounds.y() + icon_bounds.y(),
                   icon_bounds.width(), icon_bounds.height());
}

bool LauncherView::IsShowingMenu() const {
#if !defined(OS_MACOSX)
  return (launcher_menu_runner_.get() &&
       launcher_menu_runner_->IsRunning());
#endif
  return false;
}

bool LauncherView::IsShowingOverflowBubble() const {
  return overflow_bubble_.get() && overflow_bubble_->IsShowing();
}

views::View* LauncherView::GetAppListButtonView() const {
  for (int i = 0; i < model_->item_count(); ++i) {
    if (model_->items()[i].type == TYPE_APP_LIST)
      return view_model_->view_at(i);
  }

  NOTREACHED() << "Applist button not found";
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// LauncherView, FocusTraversable implementation:

views::FocusSearch* LauncherView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* LauncherView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

View* LauncherView::GetFocusTraversableParentView() {
  return this;
}

void LauncherView::LayoutToIdealBounds() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);

  if (bounds_animator_->IsAnimating())
    AnimateToIdealBounds();
  else
    views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);

  overflow_button_->SetBoundsRect(ideal_bounds.overflow_bounds);
}

void LauncherView::CalculateIdealBounds(IdealBounds* bounds) {
  int available_size = primary_axis_coordinate(width(), height());
  if (!available_size)
    return;

  int x = primary_axis_coordinate(leading_inset(), 0);
  int y = primary_axis_coordinate(0, leading_inset());
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (i < first_visible_index_) {
      view_model_->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }

    int w = primary_axis_coordinate(kLauncherPreferredSize, width());
    int h = primary_axis_coordinate(height(), kLauncherPreferredSize);
    view_model_->set_ideal_bounds(i, gfx::Rect(x, y, w, h));
    x = primary_axis_coordinate(x + w + kButtonSpacing, 0);
    y = primary_axis_coordinate(0, y + h + kButtonSpacing);
  }

  int last_view_index = view_model_->view_size() - 1;
  if (is_overflow_mode()) {
    last_visible_index_ = last_view_index;
    for (int i = 0; i < view_model_->view_size(); ++i) {
      view_model_->view_at(i)->SetVisible(
          i >= first_visible_index_ && i <= last_visible_index_);
    }
    return;
  }

  if (view_model_->view_size() > 0) {
    // Makes the first launcher button include the leading inset.
    view_model_->set_ideal_bounds(0, gfx::Rect(gfx::Size(
        primary_axis_coordinate(leading_inset() + kLauncherPreferredSize,
                                width()),
        primary_axis_coordinate(height(),
                                leading_inset() + kLauncherPreferredSize))));
  }

  bounds->overflow_bounds.set_size(gfx::Size(
      primary_axis_coordinate(kLauncherPreferredSize, width()),
      primary_axis_coordinate(height(), kLauncherPreferredSize)));
  last_visible_index_ = DetermineLastVisibleIndex(
      available_size - leading_inset() - kLauncherPreferredSize -
      kButtonSpacing - kLauncherPreferredSize);
  bool show_overflow = (last_visible_index_ < last_view_index);

  for (int i = 0; i < view_model_->view_size(); ++i) {
    view_model_->view_at(i)->SetVisible(i <= last_visible_index_);
  }

  overflow_button_->SetVisible(show_overflow);
  if (show_overflow) {
    DCHECK_NE(0, view_model_->view_size());
    if (last_visible_index_ == -1) {
      x = primary_axis_coordinate(leading_inset(), 0);
      y = primary_axis_coordinate(0, leading_inset());
    } else {
      x = primary_axis_coordinate(
          view_model_->ideal_bounds(last_visible_index_).right(), 0);
      y = primary_axis_coordinate(0,
          view_model_->ideal_bounds(last_visible_index_).bottom());
    }
    bounds->overflow_bounds.set_x(x);
    bounds->overflow_bounds.set_y(y);
  } else {
    if (overflow_bubble_.get())
      overflow_bubble_->Hide();
  }
}

int LauncherView::DetermineLastVisibleIndex(int max_value) {
  int index = view_model_->view_size() - 1;
  while (index >= 0 &&
         primary_axis_coordinate(
             view_model_->ideal_bounds(index).right(),
             view_model_->ideal_bounds(index).bottom()) > max_value) {
    index--;
  }
  return index;
}

void LauncherView::AddIconObserver(LauncherIconObserver* observer) {
  observers_.AddObserver(observer);
}

void LauncherView::RemoveIconObserver(LauncherIconObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LauncherView::AnimateToIdealBounds() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  for (int i = 0; i < view_model_->view_size(); ++i) {
    bounds_animator_->AnimateViewTo(view_model_->view_at(i),
                                    view_model_->ideal_bounds(i));
  }
  overflow_button_->SetBoundsRect(ideal_bounds.overflow_bounds);
}

views::View* LauncherView::CreateViewForItem(const LauncherItem& item) {
  views::View* view = NULL;
  switch (item.type) {
    case TYPE_TABBED: {
      TabbedLauncherButton* button =
          TabbedLauncherButton::Create(
              this,
              this,
              item.is_incognito ?
                  TabbedLauncherButton::STATE_INCOGNITO :
                  TabbedLauncherButton::STATE_NOT_INCOGNITO);
      button->SetTabImage(item.image);
      ReflectItemStatus(item, button);
      view = button;
      break;
    }

    case TYPE_APP_SHORTCUT:
    case TYPE_PLATFORM_APP:
    case TYPE_APP_PANEL: {
      LauncherButton* button = LauncherButton::Create(this, this);
      button->SetImage(item.image);
      ReflectItemStatus(item, button);
      view = button;
      break;
    }

    case TYPE_APP_LIST: {
      // TODO(dave): turn this into a LauncherButton too.
      AppListButton* button = new AppListButton(this, this);
      view = button;
      break;
    }

    case TYPE_BROWSER_SHORTCUT: {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      LauncherButton* button = LauncherButton::Create(this, this);
      int image_id = delegate_ ?
          delegate_->GetBrowserShortcutResourceId() :
          IDR_AURA_LAUNCHER_BROWSER_SHORTCUT;
      button->SetImage(*rb.GetImageNamed(image_id).ToImageSkia());
      view = button;
      break;
    }

    default:
      break;
  }
  view->set_context_menu_controller(this);

  DCHECK(view);
  ConfigureChildView(view);
  return view;
}

void LauncherView::FadeIn(views::View* view) {
  view->SetVisible(true);
  view->layer()->SetOpacity(0);
  AnimateToIdealBounds();
  bounds_animator_->SetAnimationDelegate(
      view, new FadeInAnimationDelegate(view), true);
}

void LauncherView::PrepareForDrag(Pointer pointer,
                                  const ui::LocatedEvent& event) {
  DCHECK(!dragging());
  DCHECK(drag_view_);
  drag_pointer_ = pointer;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);

  // If the item is no longer draggable, bail out.
  if (start_drag_index_ == -1 ||
      !delegate_->IsDraggable(model_->items()[start_drag_index_])) {
    CancelDrag(-1);
    return;
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void LauncherView::ContinueDrag(const ui::LocatedEvent& event) {
  // TODO: I don't think this works correctly with RTL.
  gfx::Point drag_point(event.location());
  views::View::ConvertPointToTarget(drag_view_, this, &drag_point);
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);

  // If the item is no longer draggable, bail out.
  if (current_index == -1 ||
      !delegate_->IsDraggable(model_->items()[current_index])) {
    CancelDrag(-1);
    return;
  }

  // Constrain the location to the range of valid indices for the type.
  std::pair<int, int> indices(GetDragRange(current_index));
  int last_drag_index = indices.second;
  // If the last index isn't valid, we're overflowing. Constrain to the app list
  // (which is the last visible item).
  if (last_drag_index > last_visible_index_)
    last_drag_index = last_visible_index_;
  int x = 0, y = 0;
  if (is_horizontal_alignment()) {
    x = std::max(view_model_->ideal_bounds(indices.first).x(),
                     drag_point.x() - drag_offset_);
    x = std::min(view_model_->ideal_bounds(last_drag_index).right() -
                 view_model_->ideal_bounds(current_index).width(),
                 x);
    if (drag_view_->x() == x)
      return;
    drag_view_->SetX(x);
  } else {
    y = std::max(view_model_->ideal_bounds(indices.first).y(),
                     drag_point.y() - drag_offset_);
    y = std::min(view_model_->ideal_bounds(last_drag_index).bottom() -
                 view_model_->ideal_bounds(current_index).height(),
                 y);
    if (drag_view_->y() == y)
      return;
    drag_view_->SetY(y);
  }

  int target_index =
      views::ViewModelUtils::DetermineMoveIndex(
          *view_model_, drag_view_,
          is_horizontal_alignment() ?
              views::ViewModelUtils::HORIZONTAL :
              views::ViewModelUtils::VERTICAL,
          x, y);
  target_index =
      std::min(indices.second, std::max(target_index, indices.first));
  if (target_index == current_index)
    return;

  // Change the model, the LauncherItemMoved() callback will handle the
  // |view_model_| update.
  model_->Move(current_index, target_index);
  bounds_animator_->StopAnimatingView(drag_view_);
}

bool LauncherView::SameDragType(LauncherItemType typea,
                                LauncherItemType typeb) const {
  switch (typea) {
    case TYPE_TABBED:
    case TYPE_APP_PANEL:
    case TYPE_PLATFORM_APP:
      return (typeb == TYPE_TABBED ||
          typeb == TYPE_APP_PANEL ||
          typeb == TYPE_PLATFORM_APP);
    case TYPE_APP_SHORTCUT:
    case TYPE_APP_LIST:
    case TYPE_BROWSER_SHORTCUT:
      return typeb == typea;
  }
  NOTREACHED();
  return false;
}

std::pair<int, int> LauncherView::GetDragRange(int index) {
  int min_index = -1;
  int max_index = -1;
  LauncherItemType type = model_->items()[index].type;
  for (int i = 0; i < model_->item_count(); ++i) {
    if (SameDragType(model_->items()[i].type, type)) {
      if (min_index == -1)
        min_index = i;
      max_index = i;
    }
  }
  return std::pair<int, int>(min_index, max_index);
}

void LauncherView::ConfigureChildView(views::View* view) {
  view->SetPaintToLayer(true);
  view->layer()->SetFillsBoundsOpaquely(false);
}

void LauncherView::ShowOverflowBubble() {
  int first_overflow_index = last_visible_index_ + 1;
  DCHECK_LT(first_overflow_index, view_model_->view_size() - 1);

  if (!overflow_bubble_.get())
    overflow_bubble_.reset(new OverflowBubble());

  overflow_bubble_->Show(delegate_,
                         model_,
                         overflow_button_,
                         alignment_,
                         first_overflow_index);

  Shell::GetInstance()->UpdateShelfVisibility();
}

void LauncherView::UpdateFirstButtonPadding() {
  // Creates an empty border for first launcher button to make included leading
  // inset act as the button's padding. This is only needed on button creation
  // and when shelf alignment changes.
  if (view_model_->view_size() > 0) {
    view_model_->view_at(0)->set_border(views::Border::CreateEmptyBorder(
        primary_axis_coordinate(0, leading_inset()),
        primary_axis_coordinate(leading_inset(), 0),
        0,
        0));
  }
}

bool LauncherView::ShouldHideTooltip(const gfx::Point& cursor_location) {
  gfx::Rect active_bounds;

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (child == overflow_button_)
      continue;

    // The tooltip shouldn't show over the app-list window.
    if (child == GetAppListButtonView() &&
        Shell::GetInstance()->GetAppListWindow())
      continue;

    gfx::Rect child_bounds = child->GetMirroredBounds();
    active_bounds = active_bounds.Union(child_bounds);
  }

  return !active_bounds.Contains(cursor_location);
}

int LauncherView::CancelDrag(int modified_index) {
  if (!drag_view_)
    return modified_index;
  bool was_dragging = dragging();
  int drag_view_index = view_model_->GetIndexOfView(drag_view_);
  drag_pointer_ = NONE;
  drag_view_ = NULL;
  if (drag_view_index == modified_index) {
    // The view that was being dragged is being modified. Don't do anything.
    return modified_index;
  }
  if (!was_dragging)
    return modified_index;

  // Restore previous position, tracking the position of the modified view.
  model_->Move(drag_view_index, start_drag_index_);

  // Adding a new view at end.
  if (modified_index == view_model_->view_size())
    return modified_index;

  views::View* removed_view =
      (modified_index >= 0) ? view_model_->view_at(modified_index) : NULL;
  return removed_view ? view_model_->GetIndexOfView(removed_view) : -1;
}

gfx::Size LauncherView::GetPreferredSize() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);

  const gfx::Rect last_button_bounds =
      last_visible_index_  >= first_visible_index_ ?
          view_model_->view_at(last_visible_index_)->bounds() :
          gfx::Rect(gfx::Size(kLauncherPreferredSize,
                              kLauncherPreferredSize));

  if (is_horizontal_alignment()) {
    return gfx::Size(last_button_bounds.right() + leading_inset(),
                     kLauncherPreferredSize);
  }

  return gfx::Size(kLauncherPreferredSize,
                   last_button_bounds.bottom() + leading_inset());
}

ui::EventResult LauncherView::OnGestureEvent(const ui::GestureEvent& event) {
  return gesture_handler_.ProcessGestureEvent(event) ?
      ui::ER_CONSUMED : ui::ER_UNHANDLED;
}

void LauncherView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  LayoutToIdealBounds();
  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());

  if (IsShowingOverflowBubble())
    overflow_bubble_->Hide();
}

views::FocusTraversable* LauncherView::GetPaneFocusTraversable() {
  return this;
}

void LauncherView::LauncherItemAdded(int model_index) {
  model_index = CancelDrag(model_index);
  views::View* view = CreateViewForItem(model_->items()[model_index]);
  AddChildView(view);
  // Hide the view, it'll be made visible when the animation is done. Using
  // opacity 0 here to avoid messing with CalculateIdealBounds which touches
  // the view's visibility.
  view->layer()->SetOpacity(0);
  view_model_->Add(view, model_index);

  // Give the button its ideal bounds. That way if we end up animating the
  // button before this animation completes it doesn't appear at some random
  // spot (because it was in the middle of animating from 0,0 0x0 to its
  // target).
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  view->SetBoundsRect(view_model_->ideal_bounds(model_index));

  // The first animation moves all the views to their target position. |view|
  // is hidden, so it visually appears as though we are providing space for
  // it. When done we'll fade the view in.
  AnimateToIdealBounds();
  if (model_index <= last_visible_index_) {
    bounds_animator_->SetAnimationDelegate(
        view, new StartFadeAnimationDelegate(this, view), true);
  } else {
    // Undo the hiding if animation does not run.
    view->layer()->SetOpacity(1.0f);
  }
}

void LauncherView::LauncherItemRemoved(int model_index, LauncherID id) {
#if !defined(OS_MACOSX)
  if (id == context_menu_id_)
    launcher_menu_runner_.reset();
#endif
  model_index = CancelDrag(model_index);
  views::View* view = view_model_->view_at(model_index);
  view_model_->Remove(model_index);
  // The first animation fades out the view. When done we'll animate the rest of
  // the views to their target location.
  bounds_animator_->AnimateViewTo(view, view->bounds());
  bounds_animator_->SetAnimationDelegate(
      view, new FadeOutAnimationDelegate(this, view), true);
}

void LauncherView::LauncherItemChanged(int model_index,
                                       const ash::LauncherItem& old_item) {
  const LauncherItem& item(model_->items()[model_index]);
  if (old_item.type != item.type) {
    // Type changed, swap the views.
    model_index = CancelDrag(model_index);
    scoped_ptr<views::View> old_view(view_model_->view_at(model_index));
    bounds_animator_->StopAnimatingView(old_view.get());
    view_model_->Remove(model_index);
    views::View* new_view = CreateViewForItem(item);
    AddChildView(new_view);
    view_model_->Add(new_view, model_index);
    new_view->SetBoundsRect(old_view->bounds());
    return;
  }

  views::View* view = view_model_->view_at(model_index);
  switch (item.type) {
    case TYPE_TABBED: {
      TabbedLauncherButton* button = static_cast<TabbedLauncherButton*>(view);
      gfx::Size pref = button->GetPreferredSize();
      button->SetTabImage(item.image);
      if (pref != button->GetPreferredSize())
        AnimateToIdealBounds();
      else
        button->SchedulePaint();
      ReflectItemStatus(item, button);
      break;
    }

    case TYPE_APP_SHORTCUT:
    case TYPE_PLATFORM_APP:
    case TYPE_APP_PANEL: {
      LauncherButton* button = static_cast<LauncherButton*>(view);
      ReflectItemStatus(item, button);
      button->SetImage(item.image);
      button->SchedulePaint();
      break;
    }

    default:
      break;
  }
}

void LauncherView::LauncherItemMoved(int start_index, int target_index) {
  view_model_->Move(start_index, target_index);
  AnimateToIdealBounds();
}

void LauncherView::LauncherStatusChanged() {
  AppListButton* app_list_button =
      static_cast<AppListButton*>(GetAppListButtonView());
  if (model_->status() == LauncherModel::STATUS_LOADING)
    app_list_button->StartLoadingAnimation();
  else
    app_list_button->StopLoadingAnimation();
}

void LauncherView::PointerPressedOnButton(views::View* view,
                                          Pointer pointer,
                                          const ui::LocatedEvent& event) {
  if (drag_view_)
    return;

  tooltip_->Close();
  int index = view_model_->GetIndexOfView(view);
  if (index == -1 ||
      view_model_->view_size() <= 1 ||
      !delegate_->IsDraggable(model_->items()[index]))
    return;  // View is being deleted or not draggable, ignore request.

  drag_view_ = view;
  drag_offset_ = primary_axis_coordinate(event.x(), event.y());
}

void LauncherView::PointerDraggedOnButton(views::View* view,
                                          Pointer pointer,
                                          const ui::LocatedEvent& event) {
  if (!dragging() && drag_view_ &&
      primary_axis_coordinate(abs(event.x() - drag_offset_),
                              abs(event.y() - drag_offset_)) >=
      kMinimumDragDistance) {
    PrepareForDrag(pointer, event);
  }
  if (drag_pointer_ == pointer)
    ContinueDrag(event);
}

void LauncherView::PointerReleasedOnButton(views::View* view,
                                           Pointer pointer,
                                           bool canceled) {
  if (canceled) {
    CancelDrag(-1);
  } else if (drag_pointer_ == pointer) {
    drag_pointer_ = NONE;
    drag_view_ = NULL;
    AnimateToIdealBounds();
  }
}

void LauncherView::MouseMovedOverButton(views::View* view) {
  // Mouse cursor moves doesn't make effects on the app-list button if
  // app-list bubble is already visible.
  if (view == GetAppListButtonView() &&
      Shell::GetInstance()->GetAppListWindow())
    return;

  if (!tooltip_->IsVisible())
    tooltip_->ResetTimer();
}

void LauncherView::MouseEnteredButton(views::View* view) {
  // If mouse cursor enters to the app-list button but app-list bubble is
  // already visible, we should not show the bubble in that case.
  if (view == GetAppListButtonView() &&
      Shell::GetInstance()->GetAppListWindow())
    return;

  if (tooltip_->IsVisible()) {
    tooltip_->ShowImmediately(view, GetAccessibleName(view));
  } else {
    tooltip_->ShowDelayed(view, GetAccessibleName(view));
  }
}

void LauncherView::MouseExitedButton(views::View* view) {
  if (!tooltip_->IsVisible())
    tooltip_->StopTimer();
}

ShelfAlignment LauncherView::GetShelfAlignment() const {
  return alignment_;
}

string16 LauncherView::GetAccessibleName(const views::View* view) {
  if (!delegate_)
    return string16();
  int view_index = view_model_->GetIndexOfView(view);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return string16();

  switch (model_->items()[view_index].type) {
    case TYPE_TABBED:
    case TYPE_APP_PANEL:
    case TYPE_APP_SHORTCUT:
    case TYPE_PLATFORM_APP:
      return delegate_->GetTitle(model_->items()[view_index]);

    case TYPE_APP_LIST:
      return model_->status() == LauncherModel::STATUS_LOADING ?
          l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_SYNCING_TITLE) :
          l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE);

    case TYPE_BROWSER_SHORTCUT:
      return l10n_util::GetStringUTF16(IDS_AURA_NEW_TAB);
  }
  return string16();
}

void LauncherView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  // Do not handle mouse release during drag.
  if (dragging())
    return;

  if (sender == overflow_button_) {
    ShowOverflowBubble();
    return;
  }

  if (!delegate_)
    return;
  int view_index = view_model_->GetIndexOfView(sender);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return;

  if (event.IsShiftDown())
    ui::LayerAnimator::set_slow_animation_mode(true);
  tooltip_->Close();
  switch (model_->items()[view_index].type) {
    case TYPE_TABBED:
    case TYPE_APP_PANEL:
      delegate_->ItemClicked(model_->items()[view_index], event.flags());
      break;

    case TYPE_APP_SHORTCUT:
    case TYPE_PLATFORM_APP:
      Shell::GetInstance()->delegate()->RecordUserMetricsAction(
          UMA_LAUNCHER_CLICK_ON_APP);
      delegate_->ItemClicked(model_->items()[view_index], event.flags());
      break;

    case TYPE_APP_LIST:
      Shell::GetInstance()->delegate()->RecordUserMetricsAction(
          UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON);
      Shell::GetInstance()->ToggleAppList();
      break;

    case TYPE_BROWSER_SHORTCUT:
      // Click on browser icon is counted in app clicks.
      Shell::GetInstance()->delegate()->RecordUserMetricsAction(
          UMA_LAUNCHER_CLICK_ON_APP);

      if (event.flags() & ui::EF_CONTROL_DOWN)
        delegate_->CreateNewWindow();
      else
        delegate_->CreateNewTab();
      break;
  }
  if (event.IsShiftDown())
    ui::LayerAnimator::set_slow_animation_mode(false);
}

void LauncherView::ShowContextMenuForView(views::View* source,
                                          const gfx::Point& point) {
  if (!delegate_)
    return;

  int view_index = view_model_->GetIndexOfView(source);
  if (view_index != -1 &&
      model_->items()[view_index].type == TYPE_APP_LIST) {
    view_index = -1;
  }
#if !defined(OS_MACOSX)
  scoped_ptr<ui::MenuModel> menu_model(
      view_index == -1 ?
          delegate_->CreateContextMenuForLauncher() :
          delegate_->CreateContextMenu(model_->items()[view_index]));
  if (!menu_model.get())
    return;
  AutoReset<LauncherID> reseter(
      &context_menu_id_,
      view_index == -1 ? 0 : model_->items()[view_index].id);
  views::MenuModelAdapter menu_model_adapter(menu_model.get());
  launcher_menu_runner_.reset(
      new views::MenuRunner(menu_model_adapter.CreateMenu()));
  // NOTE: if you convert to HAS_MNEMONICS be sure and update menu building
  // code.
  if (launcher_menu_runner_->RunMenuAt(
          source->GetWidget(), NULL, gfx::Rect(point, gfx::Size()),
          views::MenuItemView::TOPLEFT, 0) == views::MenuRunner::MENU_DELETED)
    return;

  Shell::GetInstance()->UpdateShelfVisibility();
#endif
}

void LauncherView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());
  PreferredSizeChanged();
}

void LauncherView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
}

}  // namespace internal
}  // namespace ash
