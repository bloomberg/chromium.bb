// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/tabbed_launcher_button.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/auto_reset.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using ui::Animation;
using views::View;

namespace ash {
namespace internal {

// Amount content is inset on the left edge.
static const int kLeadingInset = 8;

// Minimum distance before drag starts.
static const int kMinimumDragDistance = 8;

// Size between the buttons.
static const int kButtonSpacing = 4;

namespace {

// Custom FocusSearch used to navigate the launcher in the order items are in
// the ViewModel.
class LauncherFocusSearch : public views::FocusSearch {
 public:
  LauncherFocusSearch(views::ViewModel* view_model)
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
class DeleteViewAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit DeleteViewAnimationDelegate(views::View* view) : view_(view) {}
  virtual ~DeleteViewAnimationDelegate() {}

 private:
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(DeleteViewAnimationDelegate);
};

// AnimationDelegate used when inserting a new item. This steadily increases the
// opacity of the layer as the animation progress.
class FadeInAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
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
      break;
    case STATUS_RUNNING:
      button->ClearState(LauncherButton::STATE_ACTIVE);
      button->AddState(LauncherButton::STATE_RUNNING);
      break;
    case STATUS_ACTIVE:
      button->AddState(LauncherButton::STATE_ACTIVE);
      button->ClearState(LauncherButton::STATE_RUNNING);
      break;
  }
}

}  // namespace

// AnimationDelegate used when inserting a new item. This steadily decreased the
// opacity of the layer as the animation progress.
class LauncherView::FadeOutAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
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
class LauncherView::StartFadeAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
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

LauncherView::LauncherView(LauncherModel* model, LauncherDelegate* delegate)
    : model_(model),
      delegate_(delegate),
      view_model_(new views::ViewModel),
      last_visible_index_(-1),
      overflow_button_(NULL),
      dragging_(NULL),
      drag_view_(NULL),
      drag_offset_(0),
      start_drag_index_(-1),
      context_menu_id_(0),
      alignment_(SHELF_ALIGNMENT_BOTTOM) {
  DCHECK(model_);
  bounds_animator_.reset(new views::BoundsAnimator(this));
  set_context_menu_controller(this);
  focus_search_.reset(new LauncherFocusSearch(view_model_.get()));
}

LauncherView::~LauncherView() {
  model_->RemoveObserver(this);
}

void LauncherView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  model_->AddObserver(this);

  const LauncherItems& items(model_->items());
  for (LauncherItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    child->SetPaintToLayer(true);
    view_model_->Add(child, static_cast<int>(i - items.begin()));
    AddChildView(child);
  }

  overflow_button_ = new views::ImageButton(this);
  overflow_button_->set_accessibility_focusable(true);
  overflow_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_OVERFLOW).ToSkBitmap());
  overflow_button_->SetImage(
      views::CustomButton::BS_HOT,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_OVERFLOW_HOT).ToSkBitmap());
  overflow_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_OVERFLOW_PUSHED).ToSkBitmap());
  overflow_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AURA_LAUNCHER_OVERFLOW_NAME));
  overflow_button_->set_context_menu_controller(this);
  ConfigureChildView(overflow_button_);
  AddChildView(overflow_button_);

  // We'll layout when our bounds change.
}

void LauncherView::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;
  alignment_ = alignment;
  LayoutToIdealBounds();
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
  return (overflow_menu_runner_.get() &&
          overflow_menu_runner_->IsRunning()) ||
      (launcher_menu_runner_.get() &&
       launcher_menu_runner_->IsRunning());
#endif
  return false;
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
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
  overflow_button_->SetBoundsRect(ideal_bounds.overflow_bounds);
}

void LauncherView::CalculateIdealBounds(IdealBounds* bounds) {
  int available_size = primary_axis_coordinate(width(), height());
  if (!available_size)
    return;

  int x = primary_axis_coordinate(kLeadingInset, 0);
  int y = primary_axis_coordinate(0, kLeadingInset);
  for (int i = 0; i < view_model_->view_size(); ++i) {
    view_model_->set_ideal_bounds(i, gfx::Rect(
        x, y, kLauncherPreferredSize, kLauncherPreferredSize));
    x = primary_axis_coordinate(x + kLauncherPreferredSize + kButtonSpacing, 0);
    y = primary_axis_coordinate(0, y + kLauncherPreferredSize + kButtonSpacing);
  }

  bounds->overflow_bounds.set_size(
      gfx::Size(kLauncherPreferredSize, kLauncherPreferredSize));
  last_visible_index_ = DetermineLastVisibleIndex(
      available_size - kLeadingInset - kLauncherPreferredSize -
      kButtonSpacing - kLauncherPreferredSize);
  int app_list_index = view_model_->view_size() - 1;
  bool show_overflow = (last_visible_index_ + 1 < app_list_index);

  for (int i = 0; i < view_model_->view_size(); ++i) {
    view_model_->view_at(i)->SetVisible(
        i == app_list_index || i <= last_visible_index_);
  }

  overflow_button_->SetVisible(show_overflow);
  if (show_overflow) {
    DCHECK_NE(0, view_model_->view_size());
    if (last_visible_index_ == -1) {
      x = primary_axis_coordinate(kLeadingInset, 0);
      y = primary_axis_coordinate(0, kLeadingInset);
    } else {
      x = primary_axis_coordinate(
          view_model_->ideal_bounds(last_visible_index_).right(), 0);
      y = primary_axis_coordinate(0,
          view_model_->ideal_bounds(last_visible_index_).bottom());
    }
    gfx::Rect app_list_bounds = view_model_->ideal_bounds(app_list_index);
    app_list_bounds.set_x(x);
    app_list_bounds.set_y(y);
    view_model_->set_ideal_bounds(app_list_index, app_list_bounds);
    x = primary_axis_coordinate(x + kLauncherPreferredSize + kButtonSpacing, 0);
    y = primary_axis_coordinate(0, y + kLauncherPreferredSize + kButtonSpacing);
    bounds->overflow_bounds.set_x(x);
    bounds->overflow_bounds.set_y(y);
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
    case TYPE_APP_PANEL: {
      LauncherButton* button = LauncherButton::Create(this, this);
      button->SetImage(item.image);
      ReflectItemStatus(item, button);
      view = button;
      break;
    }

    case TYPE_APP_LIST: {
      // TODO[dave] turn this into a LauncherButton too.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      views::ImageButton* button = new views::ImageButton(this);
      button->SetImage(
          views::CustomButton::BS_NORMAL,
          rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST).ToSkBitmap());
      button->SetImage(
          views::CustomButton::BS_HOT,
          rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST_HOT).ToSkBitmap());
      button->SetImage(
          views::CustomButton::BS_PUSHED,
          rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST_PUSHED).ToSkBitmap());
      button->SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE));
      button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE));
      view = button;
      break;
    }

    case TYPE_BROWSER_SHORTCUT: {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      LauncherButton* button = LauncherButton::Create(this, this);
      int image_id = delegate_ ?
          delegate_->GetBrowserShortcutResourceId() :
          IDR_AURA_LAUNCHER_BROWSER_SHORTCUT;
      button->SetImage(*rb.GetImageNamed(image_id).ToSkBitmap());
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

void LauncherView::PrepareForDrag(const views::MouseEvent& event) {
  DCHECK(drag_view_);
  dragging_ = true;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);

  // If the item is no longer draggable, bail out.
  if (start_drag_index_ == -1 ||
      !delegate_->IsDraggable(model_->items()[start_drag_index_])) {
    CancelDrag(NULL);
    return;
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void LauncherView::ContinueDrag(const views::MouseEvent& event) {
  // TODO: I don't think this works correctly with RTL.
  gfx::Point drag_point(event.location());
  views::View::ConvertPointToView(drag_view_, this, &drag_point);
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);

  // If the item is no longer draggable, bail out.
  if (current_index == -1 ||
      !delegate_->IsDraggable(model_->items()[current_index])) {
    CancelDrag(NULL);
    return;
  }

  // Constrain the location to the range of valid indices for the type.
  std::pair<int,int> indices(GetDragRange(current_index));
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
  switch(typea) {
    case TYPE_TABBED:
    case TYPE_APP_PANEL:
      return (typeb == TYPE_TABBED || typeb == TYPE_APP_PANEL);
    case TYPE_APP_SHORTCUT:
    case TYPE_APP_LIST:
    case TYPE_BROWSER_SHORTCUT:
      return typeb == typea;
  }
  NOTREACHED();
  return false;
}

std::pair<int,int> LauncherView::GetDragRange(int index) {
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
  return std::pair<int,int>(min_index, max_index);
}

void LauncherView::ConfigureChildView(views::View* view) {
  view->SetPaintToLayer(true);
  view->layer()->SetFillsBoundsOpaquely(false);
}

void LauncherView::GetOverflowItems(std::vector<LauncherItem>* items) {
  int index = 0;
  while (index < view_model_->view_size() &&
         view_model_->view_at(index)->visible()) {
    index++;
  }
  while (index < view_model_->view_size()) {
    const LauncherItem& item = model_->items()[index];
    if (item.type == TYPE_TABBED ||
        item.type == TYPE_APP_PANEL ||
        item.type == TYPE_APP_SHORTCUT)
      items->push_back(item);
    index++;
  }
}

void LauncherView::ShowOverflowMenu() {
#if !defined(OS_MACOSX)
  if (!delegate_)
    return;

  std::vector<LauncherItem> items;
  GetOverflowItems(&items);
  if (items.empty())
    return;

  MenuDelegateImpl menu_delegate;
  ui::SimpleMenuModel menu_model(&menu_delegate);
  for (size_t i = 0; i < items.size(); ++i)
    menu_model.AddItem(static_cast<int>(i), delegate_->GetTitle(items[i]));
  views::MenuModelAdapter menu_adapter(&menu_model);
  overflow_menu_runner_.reset(new views::MenuRunner(menu_adapter.CreateMenu()));
  gfx::Rect bounds(overflow_button_->size());
  gfx::Point origin;
  ConvertPointToScreen(overflow_button_, &origin);
  if (overflow_menu_runner_->RunMenuAt(GetWidget(), NULL,
          gfx::Rect(origin, size()), views::MenuItemView::TOPLEFT, 0) ==
      views::MenuRunner::MENU_DELETED)
    return;

  Shell::GetInstance()->UpdateShelfVisibility();

  if (menu_delegate.activated_command_id() == -1)
    return;

  LauncherID activated_id = items[menu_delegate.activated_command_id()].id;
  LauncherItems::const_iterator window_iter = model_->ItemByID(activated_id);
  if (window_iter == model_->items().end())
    return;  // Window was deleted while menu was up.
  delegate_->ItemClicked(*window_iter, ui::EF_NONE);
#endif  // !defined(OS_MACOSX)
}

int LauncherView::CancelDrag(int modified_index) {
  if (!drag_view_)
    return modified_index;
  bool was_dragging = dragging_;
  int drag_view_index = view_model_->GetIndexOfView(drag_view_);
  dragging_ = false;
  drag_view_ = NULL;
  if (drag_view_index == modified_index) {
    // The view that was being dragged is being modified. Don't do anything.
    return modified_index;
  }
  if (!was_dragging)
    return modified_index;

  // Restore previous position, tracking the position of the modified view.
  views::View* removed_view =
      (modified_index >= 0) ? view_model_->view_at(modified_index) : NULL;
  model_->Move(drag_view_index, start_drag_index_);
  return removed_view ? view_model_->GetIndexOfView(removed_view) : -1;
}

gfx::Size LauncherView::GetPreferredSize() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  if (is_horizontal_alignment()) {
    if (view_model_->view_size() >= 2) {
      // Should always have two items.
      return gfx::Size(view_model_->ideal_bounds(1).right() + kLeadingInset,
                       kLauncherPreferredSize);
    }
    return gfx::Size(kLauncherPreferredSize * 2 + kLeadingInset * 2,
                     kLauncherPreferredSize);
  }
  if (view_model_->view_size() >= 2) {
    // Should always have two items.
    return gfx::Size(kLauncherPreferredSize,
                     view_model_->ideal_bounds(1).bottom() + kLeadingInset);
  }
  return gfx::Size(kLauncherPreferredSize,
                   kLauncherPreferredSize * 2 + kLeadingInset * 2);
}

void LauncherView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  LayoutToIdealBounds();
  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());
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

  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());
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

  // The animation will eventually update the ideal bounds, but we want to
  // force an update immediately so we can notify launcher icon observers.
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);

  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());
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
  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());
}

void LauncherView::MousePressedOnButton(views::View* view,
                                        const views::MouseEvent& event) {
  int index = view_model_->GetIndexOfView(view);
  if (index == -1 ||
      view_model_->view_size() <= 1 ||
      !delegate_->IsDraggable(model_->items()[index]))
    return;  // View is being deleted or not draggable, ignore request.

  drag_view_ = view;
  drag_offset_ = primary_axis_coordinate(event.x(), event.y());
}

void LauncherView::MouseDraggedOnButton(views::View* view,
                                        const views::MouseEvent& event) {
  if (!dragging_ && drag_view_ &&
      primary_axis_coordinate(abs(event.x() - drag_offset_),
                              abs(event.y() - drag_offset_)) >=
      kMinimumDragDistance) {
    PrepareForDrag(event);
  }
  if (dragging_)
    ContinueDrag(event);
}

void LauncherView::MouseReleasedOnButton(views::View* view,
                                         bool canceled) {
  if (canceled) {
    CancelDrag(-1);
  } else {
    dragging_ = false;
    drag_view_ = NULL;
    AnimateToIdealBounds();
  }
}

void LauncherView::MouseExitedButton(views::View* view) {
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
      return delegate_->GetTitle(model_->items()[view_index]);

    case TYPE_APP_LIST:
      return l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE);

    case TYPE_BROWSER_SHORTCUT:
      return l10n_util::GetStringUTF16(IDS_AURA_NEW_TAB);

  }
  return string16();
}

void LauncherView::ButtonPressed(views::Button* sender,
                                 const views::Event& event) {
  // Do not handle mouse release during drag.
  if (dragging_)
    return;

  if (sender == overflow_button_)
    ShowOverflowMenu();

  if (!delegate_)
    return;
  int view_index = view_model_->GetIndexOfView(sender);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return;

  switch (model_->items()[view_index].type) {
    case TYPE_TABBED:
    case TYPE_APP_PANEL:
    case TYPE_APP_SHORTCUT:
      delegate_->ItemClicked(model_->items()[view_index], event.flags());
      break;

    case TYPE_APP_LIST:
      Shell::GetInstance()->ToggleAppList();
      break;

    case TYPE_BROWSER_SHORTCUT:
      if (event.flags() & ui::EF_CONTROL_DOWN)
        delegate_->CreateNewWindow();
      else
        delegate_->CreateNewTab();
      break;
  }
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

}  // namespace internal
}  // namespace ash
