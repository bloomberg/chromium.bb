// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shelf/shelf_view.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "mash/shelf/shelf_button.h"
#include "mash/shelf/shelf_constants.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace mash {
namespace shelf {

const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM = 0;
const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT = 1;
const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT = 2;
const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT = 3;

// Default amount content is inset on the left edge.
const int kDefaultLeadingInset = 8;

// Minimum distance before drag starts.
const int kMinimumDragDistance = 8;

// The proportion of the shelf space reserved for non-panel icons. Panels
// may flow into this space but will be put into the overflow bubble if there
// is contention for the space.
const float kReservedNonPanelIconProportion = 0.67f;

/* TODO(msw): Restore functionality:
// The distance of the cursor from the outer rim of the shelf before it
// separates.
const int kRipOffDistance = 48;

// The rip off drag and drop proxy image should get scaled by this factor.
const float kDragAndDropProxyScale = 1.5f;

// The opacity represents that this partially disappeared item will get removed.
const float kDraggedImageOpacity = 0.5f;*/

namespace {

// A class to temporarily disable a given bounds animator.
class BoundsAnimatorDisabler {
 public:
  explicit BoundsAnimatorDisabler(views::BoundsAnimator* bounds_animator)
      : old_duration_(bounds_animator->GetAnimationDuration()),
        bounds_animator_(bounds_animator) {
    bounds_animator_->SetAnimationDuration(1);
  }

  ~BoundsAnimatorDisabler() {
    bounds_animator_->SetAnimationDuration(old_duration_);
  }

 private:
  // The previous animation duration.
  int old_duration_;
  // The bounds animator which gets used.
  views::BoundsAnimator* bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimatorDisabler);
};

// Custom FocusSearch used to navigate the shelf in the order items are in
// the ViewModel.
class ShelfFocusSearch : public views::FocusSearch {
 public:
  explicit ShelfFocusSearch(views::ViewModel* view_model)
      : FocusSearch(nullptr, true, true),
        view_model_(view_model) {}
  ~ShelfFocusSearch() override {}

  // views::FocusSearch overrides:
  views::View* FindNextFocusableView(
      views::View* starting_view,
      bool reverse,
      Direction direction,
      bool check_starting_view,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
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

  DISALLOW_COPY_AND_ASSIGN(ShelfFocusSearch);
};

// AnimationDelegate used when inserting a new item. This steadily increases the
// opacity of the layer as the animation progress.
class FadeInAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit FadeInAnimationDelegate(views::View* view) : view_(view) {}
  ~FadeInAnimationDelegate() override {}

  // AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(FadeInAnimationDelegate);
};

void ReflectItemStatus(const ShelfItem& item, ShelfButton* button) {
  switch (item.status) {
    case STATUS_CLOSED:
      button->ClearState(ShelfButton::STATE_ACTIVE);
      button->ClearState(ShelfButton::STATE_RUNNING);
      button->ClearState(ShelfButton::STATE_ATTENTION);
      break;
    case STATUS_RUNNING:
      button->ClearState(ShelfButton::STATE_ACTIVE);
      button->AddState(ShelfButton::STATE_RUNNING);
      button->ClearState(ShelfButton::STATE_ATTENTION);
      break;
    case STATUS_ACTIVE:
      button->AddState(ShelfButton::STATE_ACTIVE);
      button->ClearState(ShelfButton::STATE_RUNNING);
      button->ClearState(ShelfButton::STATE_ATTENTION);
      break;
    case STATUS_ATTENTION:
      button->ClearState(ShelfButton::STATE_ACTIVE);
      button->ClearState(ShelfButton::STATE_RUNNING);
      button->AddState(ShelfButton::STATE_ATTENTION);
      break;
  }
}

}  // namespace

// AnimationDelegate used when deleting an item. This steadily decreased the
// opacity of the layer as the animation progress.
class ShelfView::FadeOutAnimationDelegate : public gfx::AnimationDelegate {
 public:
  FadeOutAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host),
        view_(view) {}
  ~FadeOutAnimationDelegate() override {}

  // AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    shelf_view_->OnFadeOutAnimationEnded();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {}

 private:
  ShelfView* shelf_view_;
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(FadeOutAnimationDelegate);
};

// AnimationDelegate used to trigger fading an element in. When an item is
// inserted this delegate is attached to the animation that expands the size of
// the item.  When done it kicks off another animation to fade the item in.
class ShelfView::StartFadeAnimationDelegate : public gfx::AnimationDelegate {
 public:
  StartFadeAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host),
        view_(view) {}
  ~StartFadeAnimationDelegate() override {}

  // AnimationDelegate overrides:
  void AnimationEnded(const gfx::Animation* animation) override {
    shelf_view_->FadeIn(view_);
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
  }

 private:
  ShelfView* shelf_view_;
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(StartFadeAnimationDelegate);
};

ShelfView::ShelfView(mojo::ApplicationImpl* app)
    : app_(app),
      model_(app),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      first_visible_index_(0),
      last_visible_index_(-1),
      /* TODO(msw): Restore functionality:
      overflow_button_(nullptr),
      owner_overflow_bubble_(nullptr),*/
      tooltip_(this),
      drag_pointer_(NONE),
      drag_view_(nullptr),
      start_drag_index_(-1),
      context_menu_id_(0),
      leading_inset_(kDefaultLeadingInset),
      cancelling_drag_model_changed_(false),
      last_hidden_index_(0),
      closing_event_time_(),
      got_deleted_(nullptr),
      drag_and_drop_item_pinned_(false),
      drag_and_drop_shelf_id_(0),
      drag_replaced_view_(nullptr),
      dragged_off_shelf_(false),
      snap_back_from_rip_off_view_(nullptr),
      /* TODO(msw): Restore functionality:
      item_manager_(Shell::GetInstance()->shelf_item_delegate_manager()),*/
      overflow_mode_(false),
      main_shelf_(nullptr),
      dragged_off_from_overflow_to_shelf_(false),
      is_repost_event_(false),
      last_pressed_index_(-1) {
  bounds_animator_.reset(new views::BoundsAnimator(this));
  bounds_animator_->AddObserver(this);
  set_context_menu_controller(this);
  focus_search_.reset(new ShelfFocusSearch(&view_model_));

  model_.AddObserver(this);
  const ShelfItems& items(model_.items());
  for (ShelfItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    view_model_.Add(child, static_cast<int>(i - items.begin()));
    AddChildView(child);
  }

  /* TODO(msw): Restore functionality:
  overflow_button_ = new OverflowButton(this);
  overflow_button_->set_context_menu_controller(this);
  ConfigureChildView(overflow_button_);
  AddChildView(overflow_button_);*/

  /* TODO(msw): Add a stub apps button?
  ShelfItem app_list;
  app_list.type = TYPE_APP_LIST;
  app_list.title = base::ASCIIToUTF16("APPS");
  model()->Add(app_list);*/

  // TODO(msw): Needed to paint children as layers???
  SetPaintToLayer(true);
  set_background(views::Background::CreateSolidBackground(SK_ColorYELLOW));

  // We'll layout when our bounds change.
}

ShelfView::~ShelfView() {
  bounds_animator_->RemoveObserver(this);
  model_.RemoveObserver(this);
  // If we are inside the MenuRunner, we need to know if we were getting
  // deleted while it was running.
  if (got_deleted_)
    *got_deleted_ = true;
}

void ShelfView::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  /* TODO(msw): Restore functionality:
  overflow_button_->OnShelfAlignmentChanged();*/
  LayoutToIdealBounds();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    if (i >= first_visible_index_ && i <= last_visible_index_)
      view_model_.view_at(i)->Layout();
  }
  tooltip_.Close();
  /* TODO(msw): Restore functionality:
  if (overflow_bubble_)
    overflow_bubble_->Hide();*/
}

void ShelfView::SchedulePaintForAllButtons() {
  for (int i = 0; i < view_model_.view_size(); ++i) {
    if (i >= first_visible_index_ && i <= last_visible_index_)
      view_model_.view_at(i)->SchedulePaint();
  }
  /* TODO(msw): Restore functionality:
  if (overflow_button_ && overflow_button_->visible())
    overflow_button_->SchedulePaint();*/
}

gfx::Rect ShelfView::GetIdealBoundsOfItemIcon(ShelfID id) {
  int index = model_.ItemIndexByID(id);
  if (index == -1)
    return gfx::Rect();
  // Map all items from overflow area to the overflow button. Note that the
  // section between last_index_hidden_ and model_.FirstPanelIndex() is the
  // list of invisible panel items. However, these items are currently nowhere
  // represented and get dropped instead - see (crbug.com/378907). As such there
  // is no way to address them or place them. We therefore move them over the
  // overflow button.
  if (index > last_visible_index_ && index < model_.FirstPanelIndex())
    index = last_visible_index_ + 1;
  const gfx::Rect& ideal_bounds(view_model_.ideal_bounds(index));
  DCHECK_NE(TYPE_APP_LIST, model_.items()[index].type);
  views::View* view = view_model_.view_at(index);
  CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
  ShelfButton* button = static_cast<ShelfButton*>(view);
  gfx::Rect icon_bounds = button->GetIconBounds();
  return gfx::Rect(GetMirroredXWithWidthInView(
                       ideal_bounds.x() + icon_bounds.x(), icon_bounds.width()),
                   ideal_bounds.y() + icon_bounds.y(),
                   icon_bounds.width(),
                   icon_bounds.height());
}

void ShelfView::UpdatePanelIconPosition(ShelfID id,
                                        const gfx::Point& midpoint) {
  int current_index = model_.ItemIndexByID(id);
  int first_panel_index = model_.FirstPanelIndex();
  if (current_index < first_panel_index)
    return;

  gfx::Point midpoint_in_view(GetMirroredXInView(midpoint.x()),
                              midpoint.y());
  int target_index = current_index;
  while (target_index > first_panel_index &&
         PrimaryAxisValue(view_model_.ideal_bounds(target_index).x(),
                          view_model_.ideal_bounds(target_index).y()) >
         PrimaryAxisValue(midpoint_in_view.x(),
                          midpoint_in_view.y())) {
    --target_index;
  }
  while (target_index < view_model_.view_size() - 1 &&
         PrimaryAxisValue(view_model_.ideal_bounds(target_index).right(),
                          view_model_.ideal_bounds(target_index).bottom()) <
         PrimaryAxisValue(midpoint_in_view.x(),
                          midpoint_in_view.y())) {
    ++target_index;
  }
  if (current_index != target_index)
    model_.Move(current_index, target_index);
}

bool ShelfView::IsShowingMenu() const {
  return (launcher_menu_runner_.get() &&
       launcher_menu_runner_->IsRunning());
}

bool ShelfView::IsShowingOverflowBubble() const {
  /* TODO(msw): Restore functionality:
  return overflow_bubble_.get() && overflow_bubble_->IsShowing();*/
  return false;
}

views::View* ShelfView::GetAppListButtonView() const {
  for (int i = 0; i < model_.item_count(); ++i) {
    if (model_.items()[i].type == TYPE_APP_LIST)
      return view_model_.view_at(i);
  }

  NOTREACHED() << "Applist button not found";
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView, FocusTraversable implementation:

views::FocusSearch* ShelfView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ShelfView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

views::View* ShelfView::GetFocusTraversableParentView() {
  return this;
}

/* TODO(msw): Restore drag/drop functionality.
void ShelfView::CreateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    const gfx::Vector2d& cursor_offset_from_center,
    float scale_factor) {
  drag_replaced_view_ = replaced_view;
  drag_image_.reset(new ash::DragImageView(
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow(),
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE));
  drag_image_->SetImage(icon);
  gfx::Size size = drag_image_->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       cursor_offset_from_center;
  gfx::Rect drag_image_bounds(
      location_in_screen_coordinates - drag_image_offset_,
      size);
  drag_image_->SetBoundsInScreen(drag_image_bounds);
  drag_image_->SetWidgetVisible(true);
}

void ShelfView::UpdateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates) {
  // TODO(jennyz): Investigate why drag_image_ becomes NULL at this point per
  // crbug.com/34722, while the app list item is still being dragged around.
  if (drag_image_) {
    drag_image_->SetScreenPosition(
        location_in_screen_coordinates - drag_image_offset_);
  }
}

void ShelfView::DestroyDragIconProxy() {
  drag_image_.reset();
  drag_image_offset_ = gfx::Vector2d(0, 0);
}

bool ShelfView::StartDrag(const std::string& app_id,
                          const gfx::Point& location_in_screen_coordinates) {
  // Bail if an operation is already going on - or the cursor is not inside.
  // This could happen if mouse / touch operations overlap.
  if (drag_and_drop_shelf_id_ ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  // If the AppsGridView (which was dispatching this event) was opened by our
  // button, ShelfView dragging operations are locked and we have to unlock.
  CancelDrag(-1);
  drag_and_drop_item_pinned_ = false;
  drag_and_drop_app_id_ = app_id;
  drag_and_drop_shelf_id_ =
      delegate_->GetShelfIDForAppID(drag_and_drop_app_id_);
  // Check if the application is known and pinned - if not, we have to pin it so
  // that we can re-arrange the shelf order accordingly. Note that items have
  // to be pinned to give them the same (order) possibilities as a shortcut.
  // When an item is dragged from overflow to shelf, IsShowingOverflowBubble()
  // returns true. At this time, we don't need to pin the item.
  if (!IsShowingOverflowBubble() &&
      (!drag_and_drop_shelf_id_ ||
       !delegate_->IsAppPinned(app_id))) {
    delegate_->PinAppWithID(app_id);
    drag_and_drop_shelf_id_ =
        delegate_->GetShelfIDForAppID(drag_and_drop_app_id_);
    if (!drag_and_drop_shelf_id_)
      return false;
    drag_and_drop_item_pinned_ = true;
  }
  views::View* drag_and_drop_view = view_model_->view_at(
      model_.ItemIndexByID(drag_and_drop_shelf_id_));
  DCHECK(drag_and_drop_view);

  // Since there is already an icon presented by the caller, we hide this item
  // for now. That has to be done by reducing the size since the visibility will
  // change once a regrouping animation is performed.
  pre_drag_and_drop_size_ = drag_and_drop_view->size();
  drag_and_drop_view->SetSize(gfx::Size());

  // First we have to center the mouse cursor over the item.
  gfx::Point pt = drag_and_drop_view->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen_coordinates;
  ::wm::ConvertPointFromScreen(
      ash::wm::GetRootWindowAt(location_in_screen_coordinates), &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerPressedOnButton(drag_and_drop_view,
                         ShelfButtonHost::DRAG_AND_DROP,
                         event);

  // Drag the item where it really belongs.
  Drag(location_in_screen_coordinates);
  return true;
}

bool ShelfView::Drag(const gfx::Point& location_in_screen_coordinates) {
  if (!drag_and_drop_shelf_id_ ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  gfx::Point pt = location_in_screen_coordinates;
  views::View* drag_and_drop_view = view_model_->view_at(
      model_.ItemIndexByID(drag_and_drop_shelf_id_));
  ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen_coordinates;
  ::wm::ConvertPointFromScreen(
      ash::wm::GetRootWindowAt(location_in_screen_coordinates), &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_DRAGGED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerDraggedOnButton(drag_and_drop_view,
                         ShelfButtonHost::DRAG_AND_DROP,
                         event);
  return true;
}

void ShelfView::EndDrag(bool cancel) {
  if (!drag_and_drop_shelf_id_)
    return;

  views::View* drag_and_drop_view = view_model_->view_at(
      model_.ItemIndexByID(drag_and_drop_shelf_id_));
  PointerReleasedOnButton(
      drag_and_drop_view, ShelfButtonHost::DRAG_AND_DROP, cancel);

  // Either destroy the temporarily created item - or - make the item visible.
  if (drag_and_drop_item_pinned_ && cancel) {
    delegate_->UnpinAppWithID(drag_and_drop_app_id_);
  } else if (drag_and_drop_view) {
    if (cancel) {
      // When a hosted drag gets canceled, the item can remain in the same slot
      // and it might have moved within the bounds. In that case the item need
      // to animate back to its correct location.
      AnimateToIdealBounds();
    } else {
      drag_and_drop_view->SetSize(pre_drag_and_drop_size_);
    }
  }

  drag_and_drop_shelf_id_ = 0;
}*/

void ShelfView::LayoutToIdealBounds() {
  if (bounds_animator_->IsAnimating()) {
    AnimateToIdealBounds();
    return;
  }

  CalculateIdealBounds();
  /* TODO(msw): Restore functionality:
  gfx::Rect overflow_bounds = CalculateIdealBounds();*/
  views::ViewModelUtils::SetViewBoundsToIdealBounds(view_model_);
  /* TODO(msw): Restore functionality:
  overflow_button_->SetBoundsRect(overflow_bounds);*/
}

void ShelfView::UpdateAllButtonsVisibilityInOverflowMode() {
  // The overflow button is not shown in overflow mode.
  /* TODO(msw): Restore functionality:
  overflow_button_->SetVisible(false);*/
  DCHECK_LT(last_visible_index_, view_model_.view_size());
  for (int i = 0; i < view_model_.view_size(); ++i) {
    bool visible = i >= first_visible_index_ &&
        i <= last_visible_index_;
    // To track the dragging of |drag_view_| continuously, its visibility
    // should be always true regardless of its position.
    if (dragged_off_from_overflow_to_shelf_ &&
        view_model_.view_at(i) == drag_view_)
      view_model_.view_at(i)->SetVisible(true);
    else
      view_model_.view_at(i)->SetVisible(visible);
  }
}

gfx::Rect ShelfView::CalculateIdealBounds() {
  int available_size = PrimaryAxisValue(width(), height());
  DCHECK_EQ(model_.item_count(), view_model_.view_size());
  if (!available_size || model_.items().empty())
    return gfx::Rect();

  int x = 0;
  int y = 0;
  int button_size = kShelfButtonSize;
  int button_spacing = kShelfButtonSpacing;

  int w = PrimaryAxisValue(button_size, width());
  int h = PrimaryAxisValue(height(), button_size);
  for (int i = 0; i < view_model_.view_size(); ++i) {
    if (i < first_visible_index_) {
      view_model_.set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }

    view_model_.set_ideal_bounds(i, gfx::Rect(x, y, w, h));
    x = PrimaryAxisValue(x + w + button_spacing, x);
    y = PrimaryAxisValue(y, y + h + button_spacing);
  }

  if (is_overflow_mode()) {
    UpdateAllButtonsVisibilityInOverflowMode();
    return gfx::Rect();
  }

  // Right aligned icons.
  int end_position = available_size - button_spacing;
  x = PrimaryAxisValue(end_position, 0);
  y = PrimaryAxisValue(0, end_position);
  int first_panel_index = model_.FirstPanelIndex();
  for (int i = view_model_.view_size() - 1; i >= first_panel_index; --i) {
    x = PrimaryAxisValue(x - w - button_spacing, x);
    y = PrimaryAxisValue(y, y - h - button_spacing);
    view_model_.set_ideal_bounds(i, gfx::Rect(x, y, w, h));
    end_position = PrimaryAxisValue(x, y);
  }

  int last_button_index = first_panel_index - 1;
  if (last_button_index < 0)
    return gfx::Rect();

  // Icons on the left / top are guaranteed up to kLeftIconProportion of
  // the available space.
  int last_icon_position = PrimaryAxisValue(
      view_model_.ideal_bounds(last_button_index).right(),
      view_model_.ideal_bounds(last_button_index).bottom()) + button_size;
  int reserved_icon_space = available_size * kReservedNonPanelIconProportion;
  if (last_icon_position < reserved_icon_space)
    end_position = last_icon_position;
  else
    end_position = std::max(end_position, reserved_icon_space);

  gfx::Rect overflow_bounds(PrimaryAxisValue(w, width()),
                            PrimaryAxisValue(height(), h));

  last_visible_index_ = DetermineLastVisibleIndex(end_position - button_size);
  last_hidden_index_ = DetermineFirstVisiblePanelIndex(end_position) - 1;
  bool show_overflow = last_visible_index_ < last_button_index ||
      last_hidden_index_ >= first_panel_index;

  // Create Space for the overflow button
  if (show_overflow) {
    // The following code makes sure that platform apps icons (aligned to left /
    // top) are favored over panel apps icons (aligned to right / bottom).
    if (last_visible_index_ > 0 && last_visible_index_ < last_button_index) {
      // This condition means that we will take one platform app and replace it
      // with the overflow button and put the app in the overflow bubble.
      // This happens when the space needed for platform apps exceeds the
      // reserved area for non-panel icons,
      // (i.e. |last_icon_position| > |reserved_icon_space|).
      --last_visible_index_;
    } else if (last_hidden_index_ >= first_panel_index &&
               last_hidden_index_ < view_model_.view_size() - 1) {
      // This condition means that we will take a panel app icon and replace it
      // with the overflow button.
      // This happens when there is still room for platform apps in the reserved
      // area for non-panel icons,
      // (i.e. |last_icon_position| < |reserved_icon_space|).
      ++last_hidden_index_;
    }
  }

  for (int i = 0; i < view_model_.view_size(); ++i) {
    bool visible = i <= last_visible_index_ || i > last_hidden_index_;
    // To receive drag event continuously from |drag_view_| during the dragging
    // off from the shelf, don't make |drag_view_| invisible. It will be
    // eventually invisible and removed from the |view_model_| by
    // FinalizeRipOffDrag().
    if (dragged_off_shelf_ && view_model_.view_at(i) == drag_view_)
      continue;
    view_model_.view_at(i)->SetVisible(visible);
  }

  /* TODO(msw): Restore functionality:
  overflow_button_->SetVisible(show_overflow);*/
  if (show_overflow) {
    DCHECK_NE(0, view_model_.view_size());
    if (last_visible_index_ == -1) {
      x = 0;
      y = 0;
    } else {
      x = PrimaryAxisValue(
          view_model_.ideal_bounds(last_visible_index_).right(),
          view_model_.ideal_bounds(last_visible_index_).x());
      y = PrimaryAxisValue(
          view_model_.ideal_bounds(last_visible_index_).y(),
          view_model_.ideal_bounds(last_visible_index_).bottom());
    }
    // Set all hidden panel icon positions to be on the overflow button.
    for (int i = first_panel_index; i <= last_hidden_index_; ++i)
      view_model_.set_ideal_bounds(i, gfx::Rect(x, y, w, h));

    // Add more space between last visible item and overflow button.
    // Without this, two buttons look too close compared with other items.
    x = PrimaryAxisValue(x + button_spacing, x);
    y = PrimaryAxisValue(y, y + button_spacing);

    overflow_bounds.set_x(x);
    overflow_bounds.set_y(y);
    /* TODO(msw): Restore functionality:
    if (overflow_bubble_.get() && overflow_bubble_->IsShowing())
      UpdateOverflowRange(overflow_bubble_->shelf_view());*/
  } else {
    /* TODO(msw): Restore functionality:
    if (overflow_bubble_)
      overflow_bubble_->Hide();*/
  }
  return overflow_bounds;
}

int ShelfView::DetermineLastVisibleIndex(int max_value) const {
  int index = model_.FirstPanelIndex() - 1;
  while (index >= 0 &&
         PrimaryAxisValue(
             view_model_.ideal_bounds(index).right(),
             view_model_.ideal_bounds(index).bottom()) > max_value) {
    index--;
  }
  return index;
}

int ShelfView::DetermineFirstVisiblePanelIndex(int min_value) const {
  int index = model_.FirstPanelIndex();
  while (index < view_model_.view_size() &&
         PrimaryAxisValue(
             view_model_.ideal_bounds(index).right(),
             view_model_.ideal_bounds(index).bottom()) < min_value) {
    ++index;
  }
  return index;
}

/* TODO(msw): Restore functionality:
void ShelfView::AddIconObserver(ShelfIconObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfView::RemoveIconObserver(ShelfIconObserver* observer) {
  observers_.RemoveObserver(observer);
}*/

void ShelfView::AnimateToIdealBounds() {
  /* TODO(msw): Restore functionality:
  gfx::Rect overflow_bounds = CalculateIdealBounds();*/
  CalculateIdealBounds();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    views::View* view = view_model_.view_at(i);
    bounds_animator_->AnimateViewTo(view, view_model_.ideal_bounds(i));
    // Now that the item animation starts, we have to make sure that the
    // padding of the first gets properly transferred to the new first item.
    if (i && view->border())
      view->SetBorder(views::Border::NullBorder());
  }
   /* TODO(msw): Restore functionality:
   overflow_button_->SetBoundsRect(overflow_bounds);*/
}

views::View* ShelfView::CreateViewForItem(const ShelfItem& item) {
  views::View* view = nullptr;
  switch (item.type) {
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP_SHORTCUT:
    case TYPE_WINDOWED_APP:
    case TYPE_PLATFORM_APP:
    case TYPE_DIALOG:
    case TYPE_APP_PANEL: {
      ShelfButton* button = ShelfButton::Create(this, this);
      button->SetImage(item.image);
      ReflectItemStatus(item, button);
      view = button;
      break;
    }

    case TYPE_APP_LIST: {
      /* TODO(msw): Restore functionality:
      view = new AppListButton(this, this);*/
      view = new views::LabelButton(nullptr, item.title);
      break;
    }

    case TYPE_MOJO_APP: {
      // TODO(msw): Support item images, etc.
      ShelfButton* button = ShelfButton::Create(this, this);
      int image_resource_id = IDR_DEFAULT_FAVICON;
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      button->SetImage(*rb.GetImageSkiaNamed(image_resource_id));
      ReflectItemStatus(item, button);
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

void ShelfView::FadeIn(views::View* view) {
  view->SetVisible(true);
  view->layer()->SetOpacity(0);
  AnimateToIdealBounds();
  bounds_animator_->SetAnimationDelegate(
      view,
      scoped_ptr<gfx::AnimationDelegate>(new FadeInAnimationDelegate(view)));
}

void ShelfView::PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event) {
  DCHECK(!dragging());
  DCHECK(drag_view_);
  drag_pointer_ = pointer;
  start_drag_index_ = view_model_.GetIndexOfView(drag_view_);

  if (start_drag_index_== -1) {
    CancelDrag(-1);
    return;
  }

  // If the item is no longer draggable, bail out.
  /* TODO(msw): Restore functionality:
   ShelfItemDelegate* item_delegate = item_manager_->GetShelfItemDelegate(
       model_.items()[start_drag_index_].id);
   if (!item_delegate->IsDraggable()) {
     CancelDrag(-1);
     return;
   }*/

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void ShelfView::ContinueDrag(const ui::LocatedEvent& event) {
  // Due to a syncing operation the application might have been removed.
  // Bail if it is gone.
  int current_index = view_model_.GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);

  /* TODO(msw): Restore functionality:
   ShelfItemDelegate* item_delegate =
       item_manager_->GetShelfItemDelegate(model_.items()[current_index].id);
   if (!item_delegate->IsDraggable()) {
     CancelDrag(-1);
     return;
   }*/

  // If this is not a drag and drop host operation and not the app list item,
  // check if the item got ripped off the shelf - if it did we are done.
  if (!drag_and_drop_shelf_id_ &&
      RemovableByRipOff(current_index) != NOT_REMOVABLE) {
    if (HandleRipOffDrag(event))
      return;
    // The rip off handler could have changed the location of the item.
    current_index = view_model_.GetIndexOfView(drag_view_);
  }

  // TODO: I don't think this works correctly with RTL.
  gfx::Point drag_point(event.location());
  ConvertPointToTarget(drag_view_, this, &drag_point);

  // Constrain the location to the range of valid indices for the type.
  std::pair<int, int> indices(GetDragRange(current_index));
  int first_drag_index = indices.first;
  int last_drag_index = indices.second;
  // If the last index isn't valid, we're overflowing. Constrain to the app list
  // (which is the last visible item).
  if (first_drag_index < model_.FirstPanelIndex() &&
      last_drag_index > last_visible_index_)
    last_drag_index = last_visible_index_;
  int x = 0, y = 0;
  if (IsHorizontalAlignment()) {
    x = std::max(view_model_.ideal_bounds(indices.first).x(),
                 drag_point.x() - drag_origin_.x());
    x = std::min(view_model_.ideal_bounds(last_drag_index).right() -
                 view_model_.ideal_bounds(current_index).width(),
                 x);
    if (drag_view_->x() == x)
      return;
    drag_view_->SetX(x);
  } else {
    y = std::max(view_model_.ideal_bounds(indices.first).y(),
                 drag_point.y() - drag_origin_.y());
    y = std::min(view_model_.ideal_bounds(last_drag_index).bottom() -
                 view_model_.ideal_bounds(current_index).height(),
                 y);
    if (drag_view_->y() == y)
      return;
    drag_view_->SetY(y);
  }

  int target_index =
      views::ViewModelUtils::DetermineMoveIndex(
          view_model_, drag_view_,
          IsHorizontalAlignment() ? views::ViewModelUtils::HORIZONTAL :
                                    views::ViewModelUtils::VERTICAL,
          x, y);
  target_index =
      std::min(indices.second, std::max(target_index, indices.first));

  int first_draggable_item = 0;
  /* TODO(msw): Restore functionality:
  while (first_draggable_item < static_cast<int>(model_.items().size()) &&
         !item_manager_->GetShelfItemDelegate(
             model_.items()[first_draggable_item].id)->IsDraggable()) {
    first_draggable_item++;
  }*/

  target_index = std::max(target_index, first_draggable_item);

  if (target_index == current_index)
    return;

  // Change the model, the ShelfItemMoved() callback will handle the
  // |view_model_| update.
  model_.Move(current_index, target_index);
  bounds_animator_->StopAnimatingView(drag_view_);
}

bool ShelfView::HandleRipOffDrag(const ui::LocatedEvent& event) {
  /* TODO(msw): Restore functionality:
  int current_index = view_model_.GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);
  std::string dragged_app_id =
      delegate_->GetAppIDForShelfID(model_.items()[current_index].id);

  gfx::Point screen_location = event.root_location();
  views::View::ConvertPointToScreen(this, &screen_location);

  // To avoid ugly forwards and backwards flipping we use different constants
  // for ripping off / re-inserting the items.
  if (dragged_off_shelf_) {
    // If the shelf/overflow bubble bounds contains |screen_location| we insert
    // the item back into the shelf.
    if (GetBoundsForDragInsertInScreen().Contains(screen_location)) {
      if (dragged_off_from_overflow_to_shelf_) {
        // During the dragging an item from Shelf to Overflow, it can enter here
        // directly because both are located very closly.
        main_shelf_->EndDrag(true);
        // Stops the animation of |drag_view_| and sets its bounds explicitly
        // becase ContinueDrag() stops its animation. Without this, unexpected
        // bounds will be set.
        bounds_animator_->StopAnimatingView(drag_view_);
        int drag_view_index = view_model_.GetIndexOfView(drag_view_);
        drag_view_->SetBoundsRect(view_model_.ideal_bounds(drag_view_index));
        dragged_off_from_overflow_to_shelf_ = false;
      }
      // Destroy our proxy view item.
      DestroyDragIconProxy();
      // Re-insert the item and return simply false since the caller will handle
      // the move as in any normal case.
      dragged_off_shelf_ = false;
      drag_view_->layer()->SetOpacity(1.0f);
      // The size of Overflow bubble should be updated immediately when an item
      // is re-inserted.
      if (is_overflow_mode())
        PreferredSizeChanged();
      return false;
    } else if (is_overflow_mode() &&
               main_shelf_->GetBoundsForDragInsertInScreen().Contains(
                   screen_location)) {
      if (!dragged_off_from_overflow_to_shelf_) {
        dragged_off_from_overflow_to_shelf_ = true;
        drag_image_->SetOpacity(1.0f);
        main_shelf_->StartDrag(dragged_app_id, screen_location);
      } else {
        main_shelf_->Drag(screen_location);
      }
    } else if (dragged_off_from_overflow_to_shelf_) {
      // Makes the |drag_image_| partially disappear again.
      dragged_off_from_overflow_to_shelf_ = false;
      drag_image_->SetOpacity(kDraggedImageOpacity);
      main_shelf_->EndDrag(true);
      bounds_animator_->StopAnimatingView(drag_view_);
      int drag_view_index = view_model_.GetIndexOfView(drag_view_);
      drag_view_->SetBoundsRect(view_model_.ideal_bounds(drag_view_index));
    }
    // Move our proxy view item.
    UpdateDragIconProxy(screen_location);
    return true;
  }
  // Check if we are too far away from the shelf to enter the ripped off state.
  // Determine the distance to the shelf.
  int delta = CalculateShelfDistance(screen_location);
  if (delta > kRipOffDistance) {
    // Create a proxy view item which can be moved anywhere.
    CreateDragIconProxy(event.root_location(),
                        drag_view_->GetImage(),
                        drag_view_,
                        gfx::Vector2d(0, 0),
                        kDragAndDropProxyScale);
    drag_view_->layer()->SetOpacity(0.0f);
    dragged_off_shelf_ = true;
    if (RemovableByRipOff(current_index) == REMOVABLE) {
      // Move the item to the front of the first panel item and hide it.
      // ShelfItemMoved() callback will handle the |view_model_| update and
      // call AnimateToIdealBounds().
      if (current_index != model_.FirstPanelIndex() - 1) {
        model_.Move(current_index, model_.FirstPanelIndex() - 1);
        StartFadeInLastVisibleItem();
      } else if (is_overflow_mode()) {
        // Overflow bubble should be shrunk when an item is ripped off.
        PreferredSizeChanged();
      }
      // Make the item partially disappear to show that it will get removed if
      // dropped.
      drag_image_->SetOpacity(kDraggedImageOpacity);
    }
    return true;
  }*/
  return false;
}

void ShelfView::FinalizeRipOffDrag(bool cancel) {
  /* TODO(msw): Restore functionality:
  if (!dragged_off_shelf_)
    return;
  // Make sure we do not come in here again.
  dragged_off_shelf_ = false;

  // Coming here we should always have a |drag_view_|.
  DCHECK(drag_view_);
  int current_index = view_model_.GetIndexOfView(drag_view_);
  // If the view isn't part of the model anymore (|current_index| == -1), a sync
  // operation must have removed it. In that case we shouldn't change the model
  // and only delete the proxy image.
  if (current_index == -1) {
    DestroyDragIconProxy();
    return;
  }

  // Set to true when the animation should snap back to where it was before.
  bool snap_back = false;
  // Items which cannot be dragged off will be handled as a cancel.
  if (!cancel) {
    if (dragged_off_from_overflow_to_shelf_) {
      dragged_off_from_overflow_to_shelf_ = false;
      main_shelf_->EndDrag(false);
      drag_view_->layer()->SetOpacity(1.0f);
    } else if (RemovableByRipOff(current_index) != REMOVABLE) {
      // Make sure we do not try to remove un-removable items like items which
      // were not pinned or have to be always there.
      cancel = true;
      snap_back = true;
    } else {
      // Make sure the item stays invisible upon removal.
      drag_view_->SetVisible(false);
      std::string app_id = 0;
      std::string app_id =
          delegate_->GetAppIDForShelfID(model_.items()[current_index].id);
      delegate_->UnpinAppWithID(app_id);
    }
  }
  if (cancel || snap_back) {
    if (dragged_off_from_overflow_to_shelf_) {
      dragged_off_from_overflow_to_shelf_ = false;
      // Main shelf handles revert of dragged item.
      main_shelf_->EndDrag(true);
      drag_view_->layer()->SetOpacity(1.0f);
    } else if (!cancelling_drag_model_changed_) {
      // Only do something if the change did not come through a model change.
      gfx::Rect drag_bounds;
      gfx::Rect drag_bounds = drag_image_->GetBoundsInScreen();
      gfx::Point relative_to = GetBoundsInScreen().origin();
      gfx::Rect target(
          gfx::PointAtOffsetFromOrigin(drag_bounds.origin()- relative_to),
          drag_bounds.size());
      drag_view_->SetBoundsRect(target);
      // Hide the status from the active item since we snap it back now. Upon
      // animation end the flag gets cleared if |snap_back_from_rip_off_view_|
      // is set.
      snap_back_from_rip_off_view_ = drag_view_;
      drag_view_->AddState(ShelfButton::STATE_HIDDEN);
      // When a canceling drag model is happening, the view model is diverged
      // from the menu model and movements / animations should not be done.
      model_.Move(current_index, start_drag_index_);
      AnimateToIdealBounds();
    }
    drag_view_->layer()->SetOpacity(1.0f);
  }
  DestroyDragIconProxy();*/
}

ShelfView::RemovableState ShelfView::RemovableByRipOff(int index) const {
  DCHECK(index >= 0 && index < model_.item_count());
  ShelfItemType type = model_.items()[index].type;
  if (type == TYPE_APP_LIST || type == TYPE_DIALOG)
    return NOT_REMOVABLE;

  /* TODO(msw): Restore functionality:
  std::string app_id =
      delegate_->GetAppIDForShelfID(model_.items()[index].id);
  ShelfItemDelegate* item_delegate =
      item_manager_->GetShelfItemDelegate(model_.items()[index].id);
  if (!item_delegate->CanPin())
    return NOT_REMOVABLE;
  Note: Only pinned app shortcuts can be removed!
  return (type == TYPE_APP_SHORTCUT && delegate_->IsAppPinned(app_id)) ?
      REMOVABLE : DRAGGABLE;*/
  return REMOVABLE;
}

bool ShelfView::SameDragType(ShelfItemType typea, ShelfItemType typeb) const {
  switch (typea) {
    case TYPE_APP_SHORTCUT:
    case TYPE_BROWSER_SHORTCUT:
      return (typeb == TYPE_APP_SHORTCUT || typeb == TYPE_BROWSER_SHORTCUT);
    case TYPE_APP_LIST:
    case TYPE_PLATFORM_APP:
    case TYPE_WINDOWED_APP:
    case TYPE_MOJO_APP:
    case TYPE_APP_PANEL:
    case TYPE_DIALOG:
      return typeb == typea;
    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set.";
      return false;
  }
  NOTREACHED();
  return false;
}

std::pair<int, int> ShelfView::GetDragRange(int index) {
  int min_index = -1;
  int max_index = -1;
  ShelfItemType type = model_.items()[index].type;
  for (int i = 0; i < model_.item_count(); ++i) {
    if (SameDragType(model_.items()[i].type, type)) {
      if (min_index == -1)
        min_index = i;
      max_index = i;
    }
  }
  return std::pair<int, int>(min_index, max_index);
}

void ShelfView::ConfigureChildView(views::View* view) {
  view->SetPaintToLayer(true);
  view->layer()->SetFillsBoundsOpaquely(false);
}

void ShelfView::ToggleOverflowBubble() {
  /* TODO(msw): Restore functionality:
  if (IsShowingOverflowBubble()) {
    overflow_bubble_->Hide();
    return;
  }

  if (!overflow_bubble_)
    overflow_bubble_.reset(new OverflowBubble());

  ShelfView* overflow_view = new ShelfView(app);
  overflow_view->overflow_mode_ = true;
  overflow_view->Init();
  overflow_view->set_owner_overflow_bubble(overflow_bubble_.get());
  overflow_view->OnShelfAlignmentChanged();
  overflow_view->main_shelf_ = this;
  UpdateOverflowRange(overflow_view);

  overflow_bubble_->Show(overflow_button_, overflow_view);

  Shell::GetInstance()->UpdateShelfVisibility();*/
}

void ShelfView::OnFadeOutAnimationEnded() {
  AnimateToIdealBounds();
  StartFadeInLastVisibleItem();
}

void ShelfView::StartFadeInLastVisibleItem() {
  // If overflow button is visible and there is a valid new last item, fading
  // the new last item in after sliding animation is finished.
  /* TODO(msw): Restore functionality:
  if (overflow_button_->visible() && last_visible_index_ >= 0) {
    views::View* last_visible_view =
        view_model_.view_at(last_visible_index_);
    last_visible_view->layer()->SetOpacity(0);
    bounds_animator_->SetAnimationDelegate(
        last_visible_view,
        scoped_ptr<gfx::AnimationDelegate>(
            new StartFadeAnimationDelegate(this, last_visible_view)));
  }*/
}

void ShelfView::UpdateOverflowRange(ShelfView* overflow_view) const {
  const int first_overflow_index = last_visible_index_ + 1;
  const int last_overflow_index = last_hidden_index_;
  DCHECK_LE(first_overflow_index, last_overflow_index);
  DCHECK_LT(last_overflow_index, view_model_.view_size());

  /* TODO(msw): Restore functionality:
  overflow_view->first_visible_index_ = first_overflow_index;
  overflow_view->last_visible_index_ = last_overflow_index;*/
}

bool ShelfView::ShouldHideTooltip(const gfx::Point& cursor_location) {
  gfx::Rect active_bounds;

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    /* TODO(msw): Restore functionality:
    if (child == overflow_button_)
      continue;*/
    if (!ShouldShowTooltipForView(child))
      continue;

    gfx::Rect child_bounds = child->GetMirroredBounds();
    active_bounds.Union(child_bounds);
  }

  return !active_bounds.Contains(cursor_location);
}

gfx::Rect ShelfView::GetVisibleItemsBoundsInScreen() {
  gfx::Size preferred_size = GetPreferredSize();
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);
  ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, preferred_size);
}

gfx::Rect ShelfView::GetBoundsForDragInsertInScreen() {
  gfx::Size preferred_size;
  if (is_overflow_mode()) {
    /* TODO(msw): Restore functionality:
    DCHECK(owner_overflow_bubble_);
    gfx::Rect bubble_bounds =
        owner_overflow_bubble_->bubble_view()->GetBubbleBounds();
    preferred_size = bubble_bounds.size();*/
  } else {
    const int last_button_index = view_model_.view_size() - 1;
    gfx::Rect last_button_bounds =
        view_model_.view_at(last_button_index)->bounds();
    /* TODO(msw): Restore functionality:
    if (overflow_button_->visible() &&
        model_.GetItemIndexForType(TYPE_APP_PANEL) == -1) {
      // When overflow button is visible and shelf has no panel items,
      // last_button_bounds should be overflow button's bounds.
      last_button_bounds = overflow_button_->bounds();
    }*/

    if (IsHorizontalAlignment()) {
      preferred_size = gfx::Size(last_button_bounds.right() + leading_inset_,
                                 kShelfSize);
    } else {
      preferred_size = gfx::Size(kShelfSize,
                                 last_button_bounds.bottom() + leading_inset_);
    }
  }
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);

  // In overflow mode, we should use OverflowBubbleView as a source for
  // converting |origin| to screen coordinates. When a scroll operation is
  // occurred in OverflowBubble, the bounds of ShelfView in OverflowBubble can
  // be changed.
  /* TODO(msw): Restore functionality:
  if (is_overflow_mode())
    ConvertPointToScreen(owner_overflow_bubble_->bubble_view(), &origin);
  else
    ConvertPointToScreen(this, &origin);*/

  return gfx::Rect(origin, preferred_size);
}

int ShelfView::CancelDrag(int modified_index) {
  FinalizeRipOffDrag(true);
  if (!drag_view_)
    return modified_index;
  bool was_dragging = dragging();
  int drag_view_index = view_model_.GetIndexOfView(drag_view_);
  drag_pointer_ = NONE;
  drag_view_ = nullptr;
  if (drag_view_index == modified_index) {
    // The view that was being dragged is being modified. Don't do anything.
    return modified_index;
  }
  if (!was_dragging)
    return modified_index;

  // Restore previous position, tracking the position of the modified view.
  bool at_end = modified_index == view_model_.view_size();
  views::View* modified_view =
      (modified_index >= 0 && !at_end) ?
      view_model_.view_at(modified_index) : nullptr;
  model_.Move(drag_view_index, start_drag_index_);

  // If the modified view will be at the end of the list, return the new end of
  // the list.
  if (at_end)
    return view_model_.view_size();
  return modified_view ? view_model_.GetIndexOfView(modified_view) : -1;
}

gfx::Size ShelfView::GetPreferredSize() const {
  const_cast<ShelfView*>(this)->CalculateIdealBounds();

  int last_button_index = is_overflow_mode() ?
      last_visible_index_ : view_model_.view_size() - 1;

  // When an item is dragged off from the overflow bubble, it is moved to last
  // position and and changed to invisible. Overflow bubble size should be
  // shrunk to fit only for visible items.
  // If |dragged_off_from_overflow_to_shelf_| is set, there will be no invisible
  // items in the shelf.
  if (is_overflow_mode() &&
      dragged_off_shelf_ &&
      !dragged_off_from_overflow_to_shelf_ &&
      RemovableByRipOff(view_model_.GetIndexOfView(drag_view_)) == REMOVABLE)
    last_button_index--;

  const gfx::Rect last_button_bounds =
      last_button_index  >= first_visible_index_ ?
          view_model_.ideal_bounds(last_button_index) :
          gfx::Rect(gfx::Size(kShelfSize, kShelfSize));

  if (IsHorizontalAlignment())
    return gfx::Size(last_button_bounds.right() + leading_inset_, kShelfSize);

  return gfx::Size(kShelfSize, last_button_bounds.bottom() + leading_inset_);
}

void ShelfView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // This bounds change is produced by the shelf movement and all content has
  // to follow. Using an animation at that time would produce a time lag since
  // the animation of the BoundsAnimator has itself a delay before it arrives
  // at the required location. As such we tell the animator to go there
  // immediately.
  BoundsAnimatorDisabler disabler(bounds_animator_.get());
  LayoutToIdealBounds();
  /* TODO(msw): Restore functionality:
  FOR_EACH_OBSERVER(ShelfIconObserver, observers_,
                    OnShelfIconPositionsChanged());

  if (IsShowingOverflowBubble())
    overflow_bubble_->Hide();*/
}

views::FocusTraversable* ShelfView::GetPaneFocusTraversable() {
  return this;
}

void ShelfView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TOOLBAR;
  /* TODO(msw): Restore functionality:
  state->name = l10n_util::GetStringUTF16(IDS_ASH_SHELF_ACCESSIBLE_NAME);*/
}

/* TODO(msw): Restore functionality:
void ShelfView::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target_window = static_cast<views::View*>(event->target())
                                    ->GetWidget()
                                    ->GetNativeWindow();
  if (gesture_handler_.ProcessGestureEvent(*event, target_window))
    event->StopPropagation();
}*/

void ShelfView::ShelfItemAdded(int model_index) {
  {
    base::AutoReset<bool> cancelling_drag(
        &cancelling_drag_model_changed_, true);
    model_index = CancelDrag(model_index);
  }
  views::View* view = CreateViewForItem(model_.items()[model_index]);
  AddChildView(view);
  // Hide the view, it'll be made visible when the animation is done. Using
  // opacity 0 here to avoid messing with CalculateIdealBounds which touches
  // the view's visibility.
  view->layer()->SetOpacity(0);
  view_model_.Add(view, model_index);

  // Give the button its ideal bounds. That way if we end up animating the
  // button before this animation completes it doesn't appear at some random
  // spot (because it was in the middle of animating from 0,0 0x0 to its
  // target).
  CalculateIdealBounds();
  view->SetBoundsRect(view_model_.ideal_bounds(model_index));

  // The first animation moves all the views to their target position. |view|
  // is hidden, so it visually appears as though we are providing space for
  // it. When done we'll fade the view in.
  AnimateToIdealBounds();
  if (model_index <= last_visible_index_ ||
      model_index >= model_.FirstPanelIndex()) {
    bounds_animator_->SetAnimationDelegate(
        view,
        scoped_ptr<gfx::AnimationDelegate>(
            new StartFadeAnimationDelegate(this, view)));
  } else {
    // Undo the hiding if animation does not run.
    view->layer()->SetOpacity(1.0f);
  }
}

void ShelfView::ShelfItemRemoved(int model_index, ShelfID id) {
  if (id == context_menu_id_)
    launcher_menu_runner_.reset();
  {
    base::AutoReset<bool> cancelling_drag(
        &cancelling_drag_model_changed_, true);
    model_index = CancelDrag(model_index);
  }
  views::View* view = view_model_.view_at(model_index);
  view_model_.Remove(model_index);

  // When the overflow bubble is visible, the overflow range needs to be set
  // before CalculateIdealBounds() gets called. Otherwise CalculateIdealBounds()
  // could trigger a ShelfItemChanged() by hiding the overflow bubble and
  // since the overflow bubble is not yet synced with the ShelfModel this
  // could cause a crash.
  /* TODO(msw): Restore functionality:
  if (overflow_bubble_ && overflow_bubble_->IsShowing()) {
     last_hidden_index_ = std::min(last_hidden_index_,
                                   view_model_.view_size() - 1);
     UpdateOverflowRange(overflow_bubble_->shelf_view());
   }*/

  if (view->visible()) {
    // The first animation fades out the view. When done we'll animate the rest
    // of the views to their target location.
    bounds_animator_->AnimateViewTo(view, view->bounds());
    bounds_animator_->SetAnimationDelegate(
        view,
        scoped_ptr<gfx::AnimationDelegate>(
            new FadeOutAnimationDelegate(this, view)));
  } else {
    // We don't need to show a fade out animation for invisible |view|. When an
    // item is ripped out from the shelf, its |view| is already invisible.
    AnimateToIdealBounds();
  }

  // Close the tooltip because it isn't needed any longer and its anchor view
  // will be deleted soon.
  if (tooltip_.GetCurrentAnchorView() == view)
    tooltip_.Close();
}

void ShelfView::ShelfItemChanged(int model_index, const ShelfItem& old_item) {
  const ShelfItem& item(model_.items()[model_index]);
  if (old_item.type != item.type) {
    // Type changed, swap the views.
    model_index = CancelDrag(model_index);
    scoped_ptr<views::View> old_view(view_model_.view_at(model_index));
    bounds_animator_->StopAnimatingView(old_view.get());
    // Removing and re-inserting a view in our view model will strip the ideal
    // bounds from the item. To avoid recalculation of everything the bounds
    // get remembered and restored after the insertion to the previous value.
    gfx::Rect old_ideal_bounds = view_model_.ideal_bounds(model_index);
    view_model_.Remove(model_index);
    views::View* new_view = CreateViewForItem(item);
    AddChildView(new_view);
    view_model_.Add(new_view, model_index);
    view_model_.set_ideal_bounds(model_index, old_ideal_bounds);
    new_view->SetBoundsRect(old_view->bounds());
    return;
  }

  views::View* view = view_model_.view_at(model_index);
  switch (item.type) {
    case TYPE_BROWSER_SHORTCUT:
      // Fallthrough for the new Shelf since it needs to show the activation
      // change as well.
    case TYPE_APP_SHORTCUT:
    case TYPE_WINDOWED_APP:
    case TYPE_PLATFORM_APP:
    case TYPE_DIALOG:
    case TYPE_APP_PANEL: {
      CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
      ShelfButton* button = static_cast<ShelfButton*>(view);
      ReflectItemStatus(item, button);
      // The browser shortcut is currently not a "real" item and as such the
      // the image is bogous as well. We therefore keep the image as is for it.
      if (item.type != TYPE_BROWSER_SHORTCUT)
        button->SetImage(item.image);
      button->SchedulePaint();
      break;
    }
    case TYPE_MOJO_APP: {
      CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
      ShelfButton* button = static_cast<ShelfButton*>(view);
      button->SetTooltipText(item.title);
      break;
    }

    default:
      break;
  }
}

void ShelfView::ShelfItemMoved(int start_index, int target_index) {
  view_model_.Move(start_index, target_index);
  // When cancelling a drag due to a shelf item being added, the currently
  // dragged item is moved back to its initial position. AnimateToIdealBounds
  // will be called again when the new item is added to the |view_model_| but
  // at this time the |view_model_| is inconsistent with the |model_|.
  if (!cancelling_drag_model_changed_)
    AnimateToIdealBounds();
}

void ShelfView::ShelfStatusChanged() {
  // Nothing to do here.
}

ShelfAlignment ShelfView::GetAlignment() const {
  return alignment_;
}

bool ShelfView::IsHorizontalAlignment() const {
  return alignment_ == SHELF_ALIGNMENT_BOTTOM ||
         alignment_ == SHELF_ALIGNMENT_TOP;
}

void ShelfView::PointerPressedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (drag_view_)
    return;

  int index = view_model_.GetIndexOfView(view);
  if (index == -1)
    return;

  /* TODO(msw): Restore functionality:
   ShelfItemDelegate* item_delegate = item_manager_->GetShelfItemDelegate(
       model_.items()[index].id);
   if (view_model_.view_size() <= 1 || !item_delegate->IsDraggable())
     return;  // View is being deleted or not draggable, ignore request.*/

  // Only when the repost event occurs on the same shelf item, we should ignore
  // the call in ShelfView::ButtonPressed(...).
  is_repost_event_ = IsRepostEvent(event) && (last_pressed_index_ == index);

  CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
  drag_view_ = static_cast<ShelfButton*>(view);
  drag_origin_ = gfx::Point(event.x(), event.y());
  /* TODO(msw): Restore functionality:
  UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentUsage",
      layout_manager_->SelectValueForShelfAlignment(
          SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
          SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
          SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT,
          -1),
      SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);*/
}

void ShelfView::PointerDraggedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  // To prepare all drag types (moving an item in the shelf and dragging off),
  // we should check the x-axis and y-axis offset.
  if (!dragging() && drag_view_ &&
      ((std::abs(event.x() - drag_origin_.x()) >= kMinimumDragDistance) ||
       (std::abs(event.y() - drag_origin_.y()) >= kMinimumDragDistance))) {
    PrepareForDrag(pointer, event);
  }
  if (drag_pointer_ == pointer)
    ContinueDrag(event);
}

void ShelfView::PointerReleasedOnButton(views::View* view,
                                        Pointer pointer,
                                        bool canceled) {
  // Reset |is_repost_event| to false.
  is_repost_event_ = false;

  if (canceled) {
    CancelDrag(-1);
  } else if (drag_pointer_ == pointer) {
    FinalizeRipOffDrag(false);
    drag_pointer_ = NONE;
    AnimateToIdealBounds();
  }
  // If the drag pointer is NONE, no drag operation is going on and the
  // drag_view can be released.
  if (drag_pointer_ == NONE)
    drag_view_ = nullptr;
}

void ShelfView::MouseMovedOverButton(views::View* view) {
  if (!ShouldShowTooltipForView(view))
    return;

  if (!tooltip_.IsVisible())
    tooltip_.ResetTimer();
}

void ShelfView::MouseEnteredButton(views::View* view) {
  // TODO(msw): Fix the initial (0,0) mouse event (causes tooltip on startup).
  if (!ShouldShowTooltipForView(view))
    return;

  if (tooltip_.IsVisible())
    tooltip_.ShowImmediately(view, GetAccessibleName(view));
  else
    tooltip_.ShowDelayed(view, GetAccessibleName(view));
}

void ShelfView::MouseExitedButton(views::View* view) {
  if (!tooltip_.IsVisible())
    tooltip_.StopTimer();
}

base::string16 ShelfView::GetAccessibleName(const views::View* view) {
  int view_index = view_model_.GetIndexOfView(view);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return base::string16();
  return model_.items()[view_index].title;
}

void ShelfView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  // Do not handle mouse release during drag.
  if (dragging())
    return;

  /* TODO(msw): Restore functionality:
   if (sender == overflow_button_) {
     ToggleOverflowBubble();
     shelf_button_pressed_metric_tracker_.ButtonPressed(
         event, sender, ShelfItemDelegate::kNoAction);
     return;
   }*/

  int view_index = view_model_.GetIndexOfView(sender);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return;

  // If the menu was just closed by the same event as this one, we ignore
  // the call and don't open the menu again. See crbug.com/343005 for more
  // detail.
  if (is_repost_event_)
    return;

  // Record the index for the last pressed shelf item.
  last_pressed_index_ = view_index;

  // Don't activate the item twice on double-click. Otherwise the window starts
  // animating open due to the first click, then immediately minimizes due to
  // the second click. The user most likely intended to open or minimize the
  // item once, not do both.
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK)
    return;

  {
    /* TODO(msw): Restore functionality:
    ScopedTargetRootWindow scoped_target(
        sender->GetWidget()->GetNativeView()->GetRootWindow());*/
    // Slow down activation animations if shift key is pressed.
    scoped_ptr<ui::ScopedAnimationDurationScaleMode> slowing_animations;
    if (event.IsShiftDown()) {
      slowing_animations.reset(new ui::ScopedAnimationDurationScaleMode(
            ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
    }

    // Collect usage statistics before we decide what to do with the click.
    const ShelfItem& item(model_.items()[view_index]);
    switch (item.type) {
      case TYPE_APP_SHORTCUT:
      case TYPE_WINDOWED_APP:
      case TYPE_PLATFORM_APP:
      case TYPE_MOJO_APP:
      case TYPE_BROWSER_SHORTCUT:
        /* TODO(msw): Restore functionality:
        Shell::GetInstance()->metrics()->RecordUserMetricsAction(
            UMA_LAUNCHER_CLICK_ON_APP);*/
        break;

      case TYPE_APP_LIST:
        /* TODO(msw): Restore functionality:
        Shell::GetInstance()->metrics()->RecordUserMetricsAction(
            UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON);*/
        break;

      case TYPE_APP_PANEL:
      case TYPE_DIALOG:
        break;

      case TYPE_UNDEFINED:
        NOTREACHED() << "ShelfItemType must be set.";
        break;
    }

    // TODO(msw): Support GetShelfItemDelegate[Manager], actions, etc.
    if (item.type == TYPE_MOJO_APP)
      model_.user_window_controller()->FocusUserWindow(item.window_id);

    /* TODO(msw): Restore functionality:
     ShelfItemDelegate::PerformedAction performed_action =
         item_manager_->GetShelfItemDelegate(model_.items()[view_index].id)
             ->ItemSelected(event);

     shelf_button_pressed_metric_tracker_.ButtonPressed(event, sender,
                                                        performed_action);

     if (performed_action != ShelfItemDelegate::kNewWindowCreated)
       ShowListMenuForView(model_.items()[view_index], sender, event);*/
  }
}

bool ShelfView::ShowListMenuForView(const ShelfItem& item,
                                    views::View* source,
                                    const ui::Event& event) {
  /* TODO(msw): Restore functionality:
  ShelfItemDelegate* item_delegate =
      item_manager_->GetShelfItemDelegate(item.id);
  scoped_ptr<ui::MenuModel> list_menu_model(
      item_delegate->CreateApplicationMenu(event.flags()));

  Make sure we have a menu and it has at least two items in addition to the
  application title and the 3 spacing separators.
  if (!list_menu_model.get() || list_menu_model->GetItemCount() <= 5)
    return false;

  ShowMenu(list_menu_model.get(),
           source,
           gfx::Point(),
           false,
           ui::GetMenuSourceTypeForEvent(event));*/
  return true;
}

void ShelfView::ShowContextMenuForView(views::View* source,
                                       const gfx::Point& point,
                                       ui::MenuSourceType source_type) {
  int view_index = view_model_.GetIndexOfView(source);
  if (view_index == -1) {
    /* TODO(msw): Restore functionality:
    Shell::GetInstance()->ShowContextMenu(point, source_type);*/
    return;
  }

  /* TODO(msw): Restore functionality:
  ShelfItemDelegate* item_delegate = item_manager_->GetShelfItemDelegate(
      model_.items()[view_index].id);
  context_menu_model_.reset(item_delegate->CreateContextMenu(
      source->GetWidget()->GetNativeView()->GetRootWindow()));
  if (!context_menu_model_)
    return;

  base::AutoReset<ShelfID> reseter(
      &context_menu_id_,
      view_index == -1 ? 0 : model_.items()[view_index].id);

  ShowMenu(context_menu_model_.get(),
           source,
           point,
           true,
           source_type);*/
}

void ShelfView::ShowMenu(ui::MenuModel* menu_model,
                         views::View* source,
                         const gfx::Point& click_point,
                         bool context_menu,
                         ui::MenuSourceType source_type) {
  closing_event_time_ = base::TimeDelta();
  launcher_menu_runner_.reset(new views::MenuRunner(
      menu_model, context_menu ? views::MenuRunner::CONTEXT_MENU : 0));

  /* TODO(msw): Restore functionality:
  ScopedTargetRootWindow scoped_target(
      source->GetWidget()->GetNativeView()->GetRootWindow());*/

  // Determine the menu alignment dependent on the shelf.
  views::MenuAnchorPosition menu_alignment = views::MENU_ANCHOR_TOPLEFT;
  gfx::Rect anchor_point = gfx::Rect(click_point, gfx::Size());

  if (!context_menu) {
    // Application lists use a bubble.
    anchor_point = source->GetBoundsInScreen();

    // It is possible to invoke the menu while it is sliding into view. To cover
    // that case, the screen coordinates are offsetted by the animation delta.
    gfx::Vector2d offset =
        source->GetWidget()->GetNativeWindow()->bounds().origin() -
        source->GetWidget()->GetNativeWindow()->GetTargetBounds().origin();
    anchor_point.set_x(anchor_point.x() - offset.x());
    anchor_point.set_y(anchor_point.y() - offset.y());

    // Shelf items can have an asymmetrical border for spacing reasons.
    // Adjust anchor location for this.
    if (source->border())
      anchor_point.Inset(source->border()->GetInsets());

    menu_alignment = SelectValueForShelfAlignment(
        views::MENU_ANCHOR_BUBBLE_ABOVE, views::MENU_ANCHOR_BUBBLE_RIGHT,
        views::MENU_ANCHOR_BUBBLE_LEFT, views::MENU_ANCHOR_BUBBLE_BELOW);
  }
  // If this gets deleted while we are in the menu, the shelf will be gone
  // as well.
  bool got_deleted = false;
  got_deleted_ = &got_deleted;

  /* TODO(msw): Restore functionality:
  shelf->ForceUndimming(true);*/
  // NOTE: if you convert to HAS_MNEMONICS be sure and update menu building
  // code.
  if (launcher_menu_runner_->RunMenuAt(source->GetWidget(),
                                       nullptr,
                                       anchor_point,
                                       menu_alignment,
                                       source_type) ==
      views::MenuRunner::MENU_DELETED) {
    if (!got_deleted) {
      got_deleted_ = nullptr;
      /* TODO(msw): Restore functionality:
      shelf->ForceUndimming(false);*/
    }
    return;
  }
  got_deleted_ = nullptr;
  /* TODO(msw): Restore functionality:
  shelf->ForceUndimming(false);*/

  // If it is a context menu and we are showing overflow bubble
  // we want to hide overflow bubble.
  /* TODO(msw): Restore functionality:
  if (owner_overflow_bubble_)
    owner_overflow_bubble_->HideBubbleAndRefreshButton();*/

  // Unpinning an item will reset the |launcher_menu_runner_| before coming
  // here.
  if (launcher_menu_runner_)
    closing_event_time_ = launcher_menu_runner_->closing_event_time();
  /* TODO(msw): Restore functionality:
  Shell::GetInstance()->UpdateShelfVisibility();*/
}

void ShelfView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
  /* TODO(msw): Restore functionality:
  FOR_EACH_OBSERVER(ShelfIconObserver, observers_,
                    OnShelfIconPositionsChanged());*/
  PreferredSizeChanged();
}

void ShelfView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  if (snap_back_from_rip_off_view_ && animator == bounds_animator_.get()) {
    if (!animator->IsAnimating(snap_back_from_rip_off_view_)) {
      // Coming here the animation of the ShelfButton is finished and the
      // previously hidden status can be shown again. Since the button itself
      // might have gone away or changed locations we check that the button
      // is still in the shelf and show its status again.
      for (int index = 0; index < view_model_.view_size(); index++) {
        views::View* view = view_model_.view_at(index);
        if (view == snap_back_from_rip_off_view_) {
          CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
          ShelfButton* button = static_cast<ShelfButton*>(view);
          button->ClearState(ShelfButton::STATE_HIDDEN);
          break;
        }
      }
      snap_back_from_rip_off_view_ = nullptr;
    }
  }
}

bool ShelfView::IsRepostEvent(const ui::Event& event) {
  if (closing_event_time_ == base::TimeDelta())
    return false;

  base::TimeDelta delta =
      base::TimeDelta(event.time_stamp() - closing_event_time_);
  closing_event_time_ = base::TimeDelta();
  // If the current (press down) event is a repost event, the time stamp of
  // these two events should be the same.
  return (delta.InMilliseconds() == 0);
}

const ShelfItem* ShelfView::ShelfItemForView(const views::View* view) const {
  int view_index = view_model_.GetIndexOfView(view);
  if (view_index == -1)
    return nullptr;
  return &(model_.items()[view_index]);
}

bool ShelfView::ShouldShowTooltipForView(const views::View* view) const {
  /* TODO(msw): Restore functionality:
  if (view == GetAppListButtonView() &&
      Shell::GetInstance()->GetAppListWindow())
    return false;
  const ShelfItem* item = ShelfItemForView(view);
  if (!item)
    return true;
  return item_manager_->GetShelfItemDelegate(item->id)->ShouldShowTooltip();*/
  return true;
}

int ShelfView::CalculateShelfDistance(const gfx::Point& coordinate) const {
  const gfx::Rect bounds = GetBoundsInScreen();
  int distance = 0;
  switch (GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      distance = bounds.y() - coordinate.y();
      break;
    case SHELF_ALIGNMENT_LEFT:
      distance = coordinate.x() - bounds.right();
      break;
    case SHELF_ALIGNMENT_RIGHT:
      distance = bounds.x() - coordinate.x();
      break;
    case SHELF_ALIGNMENT_TOP:
      distance = coordinate.y() - bounds.bottom();
      break;
  }
  return distance > 0 ? distance : 0;
}

}  // namespace shelf
}  // namespace mash
