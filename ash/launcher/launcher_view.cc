// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include <algorithm>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/launcher/launcher_item_delegate.h"
#include "ash/launcher/launcher_item_delegate_manager.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_tooltip_manager.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_target_root_window.h"
#include "ash/shelf/alternate_app_list_button.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell_delegate.h"
#include "base/auto_reset.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/focus_border.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

using gfx::Animation;
using views::View;

namespace ash {
namespace internal {

// Default amount content is inset on the left edge.
const int kDefaultLeadingInset = 8;

// Minimum distance before drag starts.
const int kMinimumDragDistance = 8;

// Size between the buttons.
const int kButtonSpacing = 4;
const int kAlternateButtonSpacing = 10;

// Size allocated to for each button.
const int kButtonSize = 44;

// Additional spacing for the left and right side of icons.
const int kHorizontalIconSpacing = 2;

// Inset for items which do not have an icon.
const int kHorizontalNoIconInsetSpacing =
    kHorizontalIconSpacing + kDefaultLeadingInset;

// The proportion of the launcher space reserved for non-panel icons. Panels
// may flow into this space but will be put into the overflow bubble if there
// is contention for the space.
const float kReservedNonPanelIconProportion = 0.67f;

// This is the command id of the menu item which contains the name of the menu.
const int kCommandIdOfMenuName = 0;

// The background color of the active item in the list.
const SkColor kActiveListItemBackgroundColor = SkColorSetRGB(203 , 219, 241);

// The background color of the active & hovered item in the list.
const SkColor kFocusedActiveListItemBackgroundColor =
    SkColorSetRGB(193, 211, 236);

// The text color of the caption item in a list.
const SkColor kCaptionItemForegroundColor = SK_ColorBLACK;

// The maximum allowable length of a menu line of an application menu in pixels.
const int kMaximumAppMenuItemLength = 350;

// The distance of the cursor from the outer rim of the shelf before it
// separates / re-inserts. Note that the rip off distance is bigger then the
// re-insertion distance to avoid "flickering" between the two states.
const int kRipOffDistance = 48;
const int kReinsertDistance = 32;

// The rip off drag and drop proxy image should get scaled by this factor.
const float kDragAndDropProxyScale = 1.5f;

namespace {

// The MenuModelAdapter gets slightly changed to adapt the menu appearance to
// our requirements.
class LauncherMenuModelAdapter
    : public views::MenuModelAdapter {
 public:
  explicit LauncherMenuModelAdapter(ash::LauncherMenuModel* menu_model);

  // Overriding MenuModelAdapter's MenuDelegate implementation.
  virtual const gfx::Font* GetLabelFont(int command_id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void GetHorizontalIconMargins(int id,
                                        int icon_size,
                                        int* left_margin,
                                        int* right_margin) const OVERRIDE;
  virtual bool GetForegroundColor(int command_id,
                                  bool is_hovered,
                                  SkColor* override_color) const OVERRIDE;
  virtual bool GetBackgroundColor(int command_id,
                                  bool is_hovered,
                                  SkColor* override_color) const OVERRIDE;
  virtual int GetMaxWidthForMenu(views::MenuItemView* menu) OVERRIDE;
  virtual bool ShouldReserveSpaceForSubmenuIndicator() const OVERRIDE;

 private:
  ash::LauncherMenuModel* launcher_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(LauncherMenuModelAdapter);
};


LauncherMenuModelAdapter::LauncherMenuModelAdapter(
    ash::LauncherMenuModel* menu_model)
    : MenuModelAdapter(menu_model),
      launcher_menu_model_(menu_model) {}

const gfx::Font* LauncherMenuModelAdapter::GetLabelFont(
    int command_id) const {
  if (command_id != kCommandIdOfMenuName)
    return MenuModelAdapter::GetLabelFont(command_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return &rb.GetFont(ui::ResourceBundle::BoldFont);
}

bool LauncherMenuModelAdapter::IsCommandEnabled(int id) const {
  return id != kCommandIdOfMenuName;
}

bool LauncherMenuModelAdapter::GetForegroundColor(
    int command_id,
    bool is_hovered,
    SkColor* override_color) const {
  if (command_id != kCommandIdOfMenuName)
    return false;

  *override_color = kCaptionItemForegroundColor;
  return true;
}

bool LauncherMenuModelAdapter::GetBackgroundColor(
    int command_id,
    bool is_hovered,
    SkColor* override_color) const {
  if (!launcher_menu_model_->IsCommandActive(command_id))
    return false;

  *override_color = is_hovered ? kFocusedActiveListItemBackgroundColor :
                                 kActiveListItemBackgroundColor;
  return true;
}

void LauncherMenuModelAdapter::GetHorizontalIconMargins(
    int command_id,
    int icon_size,
    int* left_margin,
    int* right_margin) const {
  *left_margin = kHorizontalIconSpacing;
  *right_margin = (command_id != kCommandIdOfMenuName) ?
      kHorizontalIconSpacing : -(icon_size + kHorizontalNoIconInsetSpacing);
}

int LauncherMenuModelAdapter::GetMaxWidthForMenu(views::MenuItemView* menu) {
  return kMaximumAppMenuItemLength;
}

bool LauncherMenuModelAdapter::ShouldReserveSpaceForSubmenuIndicator() const {
  return false;
}

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

class LauncherButtonFocusBorder : public views::FocusBorder {
 public:
  LauncherButtonFocusBorder() {}
  virtual ~LauncherButtonFocusBorder() {}

 private:
  // views::FocusBorder overrides:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE {
    gfx::Rect rect(view.GetLocalBounds());
    rect.Inset(1, 1);
    canvas->DrawRect(rect, kFocusBorderColor);
  }

  DISALLOW_COPY_AND_ASSIGN(LauncherButtonFocusBorder);
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

// Get the event location in screen coordinates.
gfx::Point GetPositionInScreen(const gfx::Point& root_location,
                               views::View* view) {
  gfx::Point root_location_in_screen = root_location;
  aura::RootWindow* root_window =
      view->GetWidget()->GetNativeWindow()->GetRootWindow();
  aura::client::GetScreenPositionClient(root_window->GetRootWindow())->
        ConvertPointToScreen(root_window, &root_location_in_screen);
  return root_location_in_screen;
}

}  // namespace

// AnimationDelegate used when deleting an item. This steadily decreased the
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
    launcher_view_->OnFadeOutAnimationEnded();
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
      leading_inset_(kDefaultLeadingInset),
      cancelling_drag_model_changed_(false),
      last_hidden_index_(0),
      closing_event_time_(base::TimeDelta()),
      got_deleted_(NULL),
      drag_and_drop_item_pinned_(false),
      drag_and_drop_launcher_id_(0),
      dragged_off_shelf_(false),
      snap_back_from_rip_off_view_(NULL),
      item_manager_(Shell::GetInstance()->launcher_item_delegate_manager()) {
  DCHECK(model_);
  bounds_animator_.reset(new views::BoundsAnimator(this));
  bounds_animator_->AddObserver(this);
  set_context_menu_controller(this);
  focus_search_.reset(new LauncherFocusSearch(view_model_.get()));
  tooltip_.reset(new LauncherTooltipManager(
      shelf_layout_manager, this));
}

LauncherView::~LauncherView() {
  bounds_animator_->RemoveObserver(this);
  model_->RemoveObserver(this);
  // If we are inside the MenuRunner, we need to know if we were getting
  // deleted while it was running.
  if (got_deleted_)
    *got_deleted_ = true;
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
  LauncherStatusChanged();
  overflow_button_ = new OverflowButton(this);
  overflow_button_->set_context_menu_controller(this);
  ConfigureChildView(overflow_button_);
  AddChildView(overflow_button_);
  UpdateFirstButtonPadding();

  // We'll layout when our bounds change.
}

void LauncherView::OnShelfAlignmentChanged() {
  UpdateFirstButtonPadding();
  overflow_button_->OnShelfAlignmentChanged();
  LayoutToIdealBounds();
  for (int i=0; i < view_model_->view_size(); ++i) {
    // TODO: remove when AppIcon is a Launcher Button.
    if (TYPE_APP_LIST == model_->items()[i].type &&
        !ash::switches::UseAlternateShelfLayout()) {
      ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();
      static_cast<AppListButton*>(view_model_->view_at(i))->SetImageAlignment(
          shelf->SelectValueForShelfAlignment(
              views::ImageButton::ALIGN_CENTER,
              views::ImageButton::ALIGN_LEFT,
              views::ImageButton::ALIGN_RIGHT,
              views::ImageButton::ALIGN_CENTER),
          shelf->SelectValueForShelfAlignment(
              views::ImageButton::ALIGN_TOP,
              views::ImageButton::ALIGN_MIDDLE,
              views::ImageButton::ALIGN_MIDDLE,
              views::ImageButton::ALIGN_BOTTOM));
    }
    if (i >= first_visible_index_ && i <= last_visible_index_)
      view_model_->view_at(i)->Layout();
  }
  tooltip_->Close();
  if (overflow_bubble_)
    overflow_bubble_->Hide();
}

void LauncherView::SchedulePaintForAllButtons() {
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (i >= first_visible_index_ && i <= last_visible_index_)
      view_model_->view_at(i)->SchedulePaint();
  }
  if (overflow_button_ && overflow_button_->visible())
    overflow_button_->SchedulePaint();
}

gfx::Rect LauncherView::GetIdealBoundsOfItemIcon(LauncherID id) {
  int index = model_->ItemIndexByID(id);
  if (index == -1 || (index > last_visible_index_ &&
                      index < model_->FirstPanelIndex()))
    return gfx::Rect();
  const gfx::Rect& ideal_bounds(view_model_->ideal_bounds(index));
  DCHECK_NE(TYPE_APP_LIST, model_->items()[index].type);
  LauncherButton* button =
      static_cast<LauncherButton*>(view_model_->view_at(index));
  gfx::Rect icon_bounds = button->GetIconBounds();
  return gfx::Rect(GetMirroredXWithWidthInView(
                       ideal_bounds.x() + icon_bounds.x(), icon_bounds.width()),
                   ideal_bounds.y() + icon_bounds.y(),
                   icon_bounds.width(),
                   icon_bounds.height());
}

void LauncherView::UpdatePanelIconPosition(LauncherID id,
                                           const gfx::Point& midpoint) {
  int current_index = model_->ItemIndexByID(id);
  int first_panel_index = model_->FirstPanelIndex();
  if (current_index < first_panel_index)
    return;

  gfx::Point midpoint_in_view(GetMirroredXInView(midpoint.x()),
                              midpoint.y());
  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();
  int target_index = current_index;
  while (target_index > first_panel_index &&
         shelf->PrimaryAxisValue(view_model_->ideal_bounds(target_index).x(),
                                 view_model_->ideal_bounds(target_index).y()) >
         shelf->PrimaryAxisValue(midpoint_in_view.x(), midpoint_in_view.y())) {
    --target_index;
  }
  while (target_index < view_model_->view_size() - 1 &&
         shelf->PrimaryAxisValue(
             view_model_->ideal_bounds(target_index).right(),
             view_model_->ideal_bounds(target_index).bottom()) <
         shelf->PrimaryAxisValue(midpoint_in_view.x(), midpoint_in_view.y())) {
    ++target_index;
  }
  if (current_index != target_index)
    model_->Move(current_index, target_index);
}

bool LauncherView::IsShowingMenu() const {
  return (launcher_menu_runner_.get() &&
       launcher_menu_runner_->IsRunning());
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

void LauncherView::CreateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    const gfx::Vector2d& cursor_offset_from_center,
    float scale_factor) {
  drag_replaced_view_ = replaced_view;
  drag_image_.reset(new ash::internal::DragImageView(
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow()));
  drag_image_->SetImage(icon);
  gfx::Size size = drag_image_->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       cursor_offset_from_center;
  gfx::Rect drag_image_bounds(
      GetPositionInScreen(location_in_screen_coordinates,
                          drag_replaced_view_) - drag_image_offset_, size);
  drag_image_->SetBoundsInScreen(drag_image_bounds);
  drag_image_->SetWidgetVisible(true);
}

void LauncherView::UpdateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates) {
  drag_image_->SetScreenPosition(
      GetPositionInScreen(location_in_screen_coordinates,
                          drag_replaced_view_) - drag_image_offset_);
}

void LauncherView::DestroyDragIconProxy() {
  drag_image_.reset();
  drag_image_offset_ = gfx::Vector2d(0, 0);
}

bool LauncherView::StartDrag(const std::string& app_id,
                             const gfx::Point& location_in_screen_coordinates) {
  // Bail if an operation is already going on - or the cursor is not inside.
  // This could happen if mouse / touch operations overlap.
  if (drag_and_drop_launcher_id_ ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  // If the AppsGridView (which was dispatching this event) was opened by our
  // button, LauncherView dragging operations are locked and we have to unlock.
  CancelDrag(-1);
  drag_and_drop_item_pinned_ = false;
  drag_and_drop_app_id_ = app_id;
  drag_and_drop_launcher_id_ =
      delegate_->GetLauncherIDForAppID(drag_and_drop_app_id_);
  // Check if the application is known and pinned - if not, we have to pin it so
  // that we can re-arrange the launcher order accordingly. Note that items have
  // to be pinned to give them the same (order) possibilities as a shortcut.
  if (!drag_and_drop_launcher_id_ || !delegate_->IsAppPinned(app_id)) {
    delegate_->PinAppWithID(app_id);
    drag_and_drop_launcher_id_ =
        delegate_->GetLauncherIDForAppID(drag_and_drop_app_id_);
    if (!drag_and_drop_launcher_id_)
      return false;
    drag_and_drop_item_pinned_ = true;
  }
  views::View* drag_and_drop_view = view_model_->view_at(
      model_->ItemIndexByID(drag_and_drop_launcher_id_));
  DCHECK(drag_and_drop_view);

  // Since there is already an icon presented by the caller, we hide this item
  // for now. That has to be done by reducing the size since the visibility will
  // change once a regrouping animation is performed.
  pre_drag_and_drop_size_ = drag_and_drop_view->size();
  drag_and_drop_view->SetSize(gfx::Size());

  // First we have to center the mouse cursor over the item.
  gfx::Point pt = drag_and_drop_view->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(drag_and_drop_view, &pt);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                       pt, location_in_screen_coordinates, 0);
  PointerPressedOnButton(
      drag_and_drop_view, LauncherButtonHost::DRAG_AND_DROP, event);

  // Drag the item where it really belongs.
  Drag(location_in_screen_coordinates);
  return true;
}

bool LauncherView::Drag(const gfx::Point& location_in_screen_coordinates) {
  if (!drag_and_drop_launcher_id_ ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  gfx::Point pt = location_in_screen_coordinates;
  views::View* drag_and_drop_view = view_model_->view_at(
      model_->ItemIndexByID(drag_and_drop_launcher_id_));
  views::View::ConvertPointFromScreen(drag_and_drop_view, &pt);

  ui::MouseEvent event(ui::ET_MOUSE_DRAGGED, pt, gfx::Point(), 0);
  PointerDraggedOnButton(
      drag_and_drop_view, LauncherButtonHost::DRAG_AND_DROP, event);
  return true;
}

void LauncherView::EndDrag(bool cancel) {
  if (!drag_and_drop_launcher_id_)
    return;

  views::View* drag_and_drop_view = view_model_->view_at(
      model_->ItemIndexByID(drag_and_drop_launcher_id_));
  PointerReleasedOnButton(
      drag_and_drop_view, LauncherButtonHost::DRAG_AND_DROP, cancel);

  // Either destroy the temporarily created item - or - make the item visible.
  if (drag_and_drop_item_pinned_ && cancel)
    delegate_->UnpinAppWithID(drag_and_drop_app_id_);
  else if (drag_and_drop_view) {
    if (cancel) {
      // When a hosted drag gets canceled, the item can remain in the same slot
      // and it might have moved within the bounds. In that case the item need
      // to animate back to its correct location.
      AnimateToIdealBounds();
    } else {
      drag_and_drop_view->SetSize(pre_drag_and_drop_size_);
    }
  }

  drag_and_drop_launcher_id_ = 0;
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
  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();

  int available_size = shelf->PrimaryAxisValue(width(), height());
  DCHECK(model_->item_count() == view_model_->view_size());
  if (!available_size)
    return;

  int first_panel_index = model_->FirstPanelIndex();
  int last_button_index = first_panel_index - 1;

  // Initial x,y values account both leading_inset in primary
  // coordinate and secondary coordinate based on the dynamic edge of the
  // launcher (eg top edge on bottom-aligned launcher).
  int inset = ash::switches::UseAlternateShelfLayout() ? 0 : leading_inset();
  int x = shelf->SelectValueForShelfAlignment(inset, 0, 0, inset);
  int y = shelf->SelectValueForShelfAlignment(0, inset, inset, 0);

  int button_size = ash::switches::UseAlternateShelfLayout() ?
      kButtonSize : kLauncherPreferredSize;
  int button_spacing = ash::switches::UseAlternateShelfLayout() ?
      kAlternateButtonSpacing : kButtonSpacing;

  int w = shelf->PrimaryAxisValue(button_size, width());
  int h = shelf->PrimaryAxisValue(height(), button_size);
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (i < first_visible_index_) {
      view_model_->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }

    view_model_->set_ideal_bounds(i, gfx::Rect(x, y, w, h));
    if (i != last_button_index) {
      x = shelf->PrimaryAxisValue(x + w + button_spacing, x);
      y = shelf->PrimaryAxisValue(y, y + h + button_spacing);
    }
  }

  if (is_overflow_mode()) {
    DCHECK_LT(last_visible_index_, view_model_->view_size());
    for (int i = 0; i < view_model_->view_size(); ++i) {
      bool visible = i >= first_visible_index_ &&
          i <= last_visible_index_;
      if (!ash::switches::UseAlternateShelfLayout())
        visible &= i != last_button_index;
      view_model_->view_at(i)->SetVisible(visible);
    }
    return;
  }

  // To address Fitt's law, we make the first launcher button include the
  // leading inset (if there is one).
  if (!ash::switches::UseAlternateShelfLayout()) {
    if (view_model_->view_size() > 0) {
      view_model_->set_ideal_bounds(0, gfx::Rect(gfx::Size(
          shelf->PrimaryAxisValue(inset + w, w),
          shelf->PrimaryAxisValue(h, inset + h))));
    }
  }

  // Right aligned icons.
  int end_position = available_size - button_spacing;
  x = shelf->PrimaryAxisValue(end_position, 0);
  y = shelf->PrimaryAxisValue(0, end_position);
  for (int i = view_model_->view_size() - 1;
       i >= first_panel_index; --i) {
    x = shelf->PrimaryAxisValue(x - w - button_spacing, x);
    y = shelf->PrimaryAxisValue(y, y - h - button_spacing);
    view_model_->set_ideal_bounds(i, gfx::Rect(x, y, w, h));
    end_position = shelf->PrimaryAxisValue(x, y);
  }

  // Icons on the left / top are guaranteed up to kLeftIconProportion of
  // the available space.
  int last_icon_position = shelf->PrimaryAxisValue(
      view_model_->ideal_bounds(last_button_index).right(),
      view_model_->ideal_bounds(last_button_index).bottom())
      + button_size + inset;
  if (!ash::switches::UseAlternateShelfLayout())
      last_icon_position += button_size;
  int reserved_icon_space = available_size * kReservedNonPanelIconProportion;
  if (last_icon_position < reserved_icon_space)
    end_position = last_icon_position;
  else
    end_position = std::max(end_position, reserved_icon_space);

  bounds->overflow_bounds.set_size(gfx::Size(
      shelf->PrimaryAxisValue(w, width()),
      shelf->PrimaryAxisValue(height(), h)));
  if (ash::switches::UseAlternateShelfLayout())
    last_visible_index_ = DetermineLastVisibleIndex(
        end_position - button_size);
  else
    last_visible_index_ = DetermineLastVisibleIndex(
        end_position - inset - 2 * button_size);
  last_hidden_index_ = DetermineFirstVisiblePanelIndex(end_position) - 1;
  bool show_overflow =
      ((ash::switches::UseAlternateShelfLayout() ? 0 : 1) +
      last_visible_index_ < last_button_index ||
      last_hidden_index_ >= first_panel_index);

  // Create Space for the overflow button
  if (show_overflow && ash::switches::UseAlternateShelfLayout() &&
      last_visible_index_ > 0 && last_visible_index_ < last_button_index)
    --last_visible_index_;
  for (int i = 0; i < view_model_->view_size(); ++i) {
    bool visible = i <= last_visible_index_ || i > last_hidden_index_;
    // Always show the app list.
    if (!ash::switches::UseAlternateShelfLayout())
      visible |= (i == last_button_index);
    view_model_->view_at(i)->SetVisible(visible);
  }

  overflow_button_->SetVisible(show_overflow);
  if (show_overflow) {
    DCHECK_NE(0, view_model_->view_size());
    if (last_visible_index_ == -1) {
      x = shelf->SelectValueForShelfAlignment(inset, 0, 0, inset);
      y = shelf->SelectValueForShelfAlignment(0, inset, inset, 0);
    } else if (last_visible_index_ == last_button_index
        && !ash::switches::UseAlternateShelfLayout()) {
      x = view_model_->ideal_bounds(last_visible_index_).x();
      y = view_model_->ideal_bounds(last_visible_index_).y();
    } else {
      x = shelf->PrimaryAxisValue(
          view_model_->ideal_bounds(last_visible_index_).right(),
          view_model_->ideal_bounds(last_visible_index_).x());
      y = shelf->PrimaryAxisValue(
          view_model_->ideal_bounds(last_visible_index_).y(),
          view_model_->ideal_bounds(last_visible_index_).bottom());
    }
    // Set all hidden panel icon positions to be on the overflow button.
    for (int i = first_panel_index; i <= last_hidden_index_; ++i)
      view_model_->set_ideal_bounds(i, gfx::Rect(x, y, w, h));

    bounds->overflow_bounds.set_x(x);
    bounds->overflow_bounds.set_y(y);
    if (!ash::switches::UseAlternateShelfLayout()) {
      // Position app list after overflow button.
      gfx::Rect app_list_bounds = view_model_->ideal_bounds(last_button_index);

      x = shelf->PrimaryAxisValue(x + w + button_spacing, x);
      y = shelf->PrimaryAxisValue(y, y + h + button_spacing);
      app_list_bounds.set_x(x);
      app_list_bounds.set_y(y);
      view_model_->set_ideal_bounds(last_button_index, app_list_bounds);
    }
    if (overflow_bubble_.get() && overflow_bubble_->IsShowing())
      UpdateOverflowRange(overflow_bubble_->launcher_view());
  } else {
    if (overflow_bubble_)
      overflow_bubble_->Hide();
  }
}

int LauncherView::DetermineLastVisibleIndex(int max_value) const {
  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();

  int index = model_->FirstPanelIndex() - 1;
  while (index >= 0 &&
         shelf->PrimaryAxisValue(
             view_model_->ideal_bounds(index).right(),
             view_model_->ideal_bounds(index).bottom()) > max_value) {
    index--;
  }
  return index;
}

int LauncherView::DetermineFirstVisiblePanelIndex(int min_value) const {
  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();

  int index = model_->FirstPanelIndex();
  while (index < view_model_->view_size() &&
         shelf->PrimaryAxisValue(
             view_model_->ideal_bounds(index).right(),
             view_model_->ideal_bounds(index).bottom()) < min_value) {
    ++index;
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
    View* view = view_model_->view_at(i);
    bounds_animator_->AnimateViewTo(view, view_model_->ideal_bounds(i));
    // Now that the item animation starts, we have to make sure that the
    // padding of the first gets properly transferred to the new first item.
    if (i && view->border())
      view->set_border(NULL);
    else if (!i && !view->border())
      UpdateFirstButtonPadding();
  }
  overflow_button_->SetBoundsRect(ideal_bounds.overflow_bounds);
}

views::View* LauncherView::CreateViewForItem(const LauncherItem& item) {
  views::View* view = NULL;
  switch (item.type) {
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP_SHORTCUT:
    case TYPE_WINDOWED_APP:
    case TYPE_PLATFORM_APP:
    case TYPE_APP_PANEL: {
      LauncherButton* button = LauncherButton::Create(
          this, this, tooltip_->shelf_layout_manager());
      button->SetImage(item.image);
      ReflectItemStatus(item, button);
      view = button;
      break;
    }

    case TYPE_APP_LIST: {
      if (ash::switches::UseAlternateShelfLayout()) {
        view = new AlternateAppListButton(this, this,
            tooltip_->shelf_layout_manager()->shelf_widget());
      } else {
        // TODO(dave): turn this into a LauncherButton too.
        AppListButton* button = new AppListButton(this, this);
        ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();
        button->SetImageAlignment(
            shelf->SelectValueForShelfAlignment(
                views::ImageButton::ALIGN_CENTER,
                views::ImageButton::ALIGN_LEFT,
                views::ImageButton::ALIGN_RIGHT,
                views::ImageButton::ALIGN_CENTER),
            shelf->SelectValueForShelfAlignment(
                views::ImageButton::ALIGN_TOP,
                views::ImageButton::ALIGN_MIDDLE,
                views::ImageButton::ALIGN_MIDDLE,
                views::ImageButton::ALIGN_BOTTOM));
        view = button;
      }
      break;
    }

    default:
      break;
  }
  view->set_context_menu_controller(this);
  view->set_focus_border(new LauncherButtonFocusBorder);

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

  if (start_drag_index_== -1) {
    CancelDrag(-1);
    return;
  }

  // If the item is no longer draggable, bail out.
  LauncherItemDelegate* item_delegate = item_manager_->GetLauncherItemDelegate(
      model_->items()[start_drag_index_].type);
  if (!item_delegate->IsDraggable(model_->items()[start_drag_index_])) {
    CancelDrag(-1);
    return;
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void LauncherView::ContinueDrag(const ui::LocatedEvent& event) {
  // Due to a syncing operation the application might have been removed.
  // Bail if it is gone.
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);

  LauncherItemDelegate* item_delegate = item_manager_->GetLauncherItemDelegate(
      model_->items()[current_index].type);
  if (!item_delegate->IsDraggable(model_->items()[current_index])) {
    CancelDrag(-1);
    return;
  }

  // If this is not a drag and drop host operation and not the app list item,
  // check if the item got ripped off the shelf - if it did we are done.
  if (!drag_and_drop_launcher_id_ && ash::switches::UseDragOffShelf() &&
      RemovableByRipOff(current_index) != NOT_REMOVABLE) {
    if (HandleRipOffDrag(event))
      return;
    // The rip off handler could have changed the location of the item.
    current_index = view_model_->GetIndexOfView(drag_view_);
  }

  // TODO: I don't think this works correctly with RTL.
  gfx::Point drag_point(event.location());
  views::View::ConvertPointToTarget(drag_view_, this, &drag_point);

  // Constrain the location to the range of valid indices for the type.
  std::pair<int, int> indices(GetDragRange(current_index));
  int first_drag_index = indices.first;
  int last_drag_index = indices.second;
  // If the last index isn't valid, we're overflowing. Constrain to the app list
  // (which is the last visible item).
  if (first_drag_index < model_->FirstPanelIndex() &&
      last_drag_index > last_visible_index_)
    last_drag_index = last_visible_index_;
  int x = 0, y = 0;
  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();
  if (shelf->IsHorizontalAlignment()) {
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
          shelf->IsHorizontalAlignment() ?
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

bool LauncherView::HandleRipOffDrag(const ui::LocatedEvent& event) {
  // Determine the distance to the shelf.
  int delta = CalculateShelfDistance(event.root_location());
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);
  // To avoid ugly forwards and backwards flipping we use different constants
  // for ripping off / re-inserting the items.
  if (dragged_off_shelf_) {
    // If the re-insertion distance is undercut we insert the item back into
    // the shelf. Note that the reinsertion value is slightly smaller then the
    // rip off distance to avoid flickering.
    if (delta < kReinsertDistance) {
      // Destroy our proxy view item.
      DestroyDragIconProxy();
      // Re-insert the item and return simply false since the caller will handle
      // the move as in any normal case.
      dragged_off_shelf_ = false;
      drag_view_->layer()->SetOpacity(1.0f);
      return false;
    }
    // Move our proxy view item.
    UpdateDragIconProxy(event.root_location());
    return true;
  }
  // Check if we are too far away from the shelf to enter the ripped off state.
  if (delta > kRipOffDistance) {
    // Create a proxy view item which can be moved anywhere.
    LauncherButton* button = static_cast<LauncherButton*>(drag_view_);
    CreateDragIconProxy(event.root_location(),
                        button->GetImage(),
                        drag_view_,
                        gfx::Vector2d(0, 0),
                        kDragAndDropProxyScale);
    drag_view_->layer()->SetOpacity(0.0f);
    if (RemovableByRipOff(current_index) == REMOVABLE) {
      // Move the item to the end of the launcher and hide it.
      model_->Move(current_index, model_->item_count() - 1);
      AnimateToIdealBounds();
      // Make the item partially disappear to show that it will get removed if
      // dropped.
      drag_image_->SetOpacity(0.5f);
    }
    dragged_off_shelf_ = true;
    return true;
  }
  return false;
}

void LauncherView::FinalizeRipOffDrag(bool cancel) {
  if (!dragged_off_shelf_)
    return;
  // Make sure we do not come in here again.
  dragged_off_shelf_ = false;

  // Coming here we should always have a |drag_view_|.
  DCHECK(drag_view_);
  int current_index = view_model_->GetIndexOfView(drag_view_);
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
    // Make sure we do not try to remove un-removable items like items which
    // were not pinned or have to be always there.
    if (RemovableByRipOff(current_index) != REMOVABLE) {
      cancel = true;
      snap_back = true;
    } else {
      // Make sure the item stays invisible upon removal.
      drag_view_->SetVisible(false);
      std::string app_id =
          delegate_->GetAppIDForLauncherID(model_->items()[current_index].id);
      delegate_->UnpinAppWithID(app_id);
    }
  }
  if (cancel || snap_back) {
    if (!cancelling_drag_model_changed_) {
      // Only do something if the change did not come through a model change.
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
      LauncherButton* button = static_cast<LauncherButton*>(drag_view_);
      button->AddState(LauncherButton::STATE_HIDDEN);
      // When a canceling drag model is happening, the view model is diverged
      // from the menu model and movements / animations should not be done.
      model_->Move(current_index, start_drag_index_);
      AnimateToIdealBounds();
    }
    drag_view_->layer()->SetOpacity(1.0f);
  }
  DestroyDragIconProxy();
}

LauncherView::RemovableState LauncherView::RemovableByRipOff(int index) {
  LauncherItemType type = model_->items()[index].type;
  if (type == TYPE_APP_LIST || !delegate_->CanPin())
    return NOT_REMOVABLE;
  std::string app_id =
      delegate_->GetAppIDForLauncherID(model_->items()[index].id);
  // Note: Only pinned app shortcuts can be removed!
  return (type == TYPE_APP_SHORTCUT && delegate_->IsAppPinned(app_id)) ?
      REMOVABLE : DRAGGABLE;
}

bool LauncherView::SameDragType(LauncherItemType typea,
                                LauncherItemType typeb) const {
  switch (typea) {
    case TYPE_APP_SHORTCUT:
    case TYPE_BROWSER_SHORTCUT:
      return (typeb == TYPE_APP_SHORTCUT || typeb == TYPE_BROWSER_SHORTCUT);
    case TYPE_APP_LIST:
    case TYPE_PLATFORM_APP:
    case TYPE_WINDOWED_APP:
    case TYPE_APP_PANEL:
      return typeb == typea;
    case TYPE_UNDEFINED:
      NOTREACHED() << "LauncherItemType must be set.";
      return false;
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

void LauncherView::ToggleOverflowBubble() {
  if (IsShowingOverflowBubble()) {
    overflow_bubble_->Hide();
    return;
  }

  if (!overflow_bubble_)
    overflow_bubble_.reset(new OverflowBubble());

  LauncherView* overflow_view = new LauncherView(
      model_, delegate_, tooltip_->shelf_layout_manager());
  overflow_view->Init();
  overflow_view->OnShelfAlignmentChanged();
  UpdateOverflowRange(overflow_view);

  overflow_bubble_->Show(overflow_button_, overflow_view);

  Shell::GetInstance()->UpdateShelfVisibility();
}

void LauncherView::UpdateFirstButtonPadding() {
  if (ash::switches::UseAlternateShelfLayout())
    return;

  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();

  // Creates an empty border for first launcher button to make included leading
  // inset act as the button's padding. This is only needed on button creation
  // and when shelf alignment changes.
  if (view_model_->view_size() > 0) {
    view_model_->view_at(0)->set_border(views::Border::CreateEmptyBorder(
        shelf->PrimaryAxisValue(0, leading_inset()),
        shelf->PrimaryAxisValue(leading_inset(), 0),
        0,
        0));
  }
}

void LauncherView::OnFadeOutAnimationEnded() {
  AnimateToIdealBounds();

  // If overflow button is visible and there is a valid new last item, fading
  // the new last item in after sliding animation is finished.
  if (overflow_button_->visible() && last_visible_index_ >= 0) {
    views::View* last_visible_view = view_model_->view_at(last_visible_index_);
    last_visible_view->layer()->SetOpacity(0);
    bounds_animator_->SetAnimationDelegate(
        last_visible_view,
        new LauncherView::StartFadeAnimationDelegate(this, last_visible_view),
        true);
  }
}

void LauncherView::UpdateOverflowRange(LauncherView* overflow_view) {
  const int first_overflow_index = last_visible_index_ + 1;
  const int last_overflow_index = last_hidden_index_;
  DCHECK_LE(first_overflow_index, last_overflow_index);
  DCHECK_LT(last_overflow_index, view_model_->view_size());

  overflow_view->first_visible_index_ = first_overflow_index;
  overflow_view->last_visible_index_ = last_overflow_index;
}

bool LauncherView::ShouldHideTooltip(const gfx::Point& cursor_location) {
  gfx::Rect active_bounds;

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (child == overflow_button_)
      continue;
    if (!ShouldShowTooltipForView(child))
      continue;

    gfx::Rect child_bounds = child->GetMirroredBounds();
    active_bounds.Union(child_bounds);
  }

  return !active_bounds.Contains(cursor_location);
}

int LauncherView::CancelDrag(int modified_index) {
  FinalizeRipOffDrag(true);
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
  bool at_end = modified_index == view_model_->view_size();
  views::View* modified_view =
      (modified_index >= 0 && !at_end) ?
      view_model_->view_at(modified_index) : NULL;
  model_->Move(drag_view_index, start_drag_index_);

  // If the modified view will be at the end of the list, return the new end of
  // the list.
  if (at_end)
    return view_model_->view_size();
  return modified_view ? view_model_->GetIndexOfView(modified_view) : -1;
}

gfx::Size LauncherView::GetPreferredSize() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);

  const int app_list_index = view_model_->view_size() - 1;
  const int last_button_index = is_overflow_mode() ?
      last_visible_index_ : app_list_index;
  const gfx::Rect last_button_bounds =
      last_button_index  >= first_visible_index_ ?
          view_model_->view_at(last_button_index)->bounds() :
          gfx::Rect(gfx::Size(kLauncherPreferredSize,
                              kLauncherPreferredSize));

  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();

  if (shelf->IsHorizontalAlignment()) {
    return gfx::Size(last_button_bounds.right() + leading_inset(),
                     kLauncherPreferredSize);
  }

  return gfx::Size(kLauncherPreferredSize,
                   last_button_bounds.bottom() + leading_inset());
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

void LauncherView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TOOLBAR;
  state->name = l10n_util::GetStringUTF16(IDS_ASH_SHELF_ACCESSIBLE_NAME);
}

void LauncherView::OnGestureEvent(ui::GestureEvent* event) {
  if (gesture_handler_.ProcessGestureEvent(*event))
    event->StopPropagation();
}

void LauncherView::LauncherItemAdded(int model_index) {
  {
    base::AutoReset<bool> cancelling_drag(
        &cancelling_drag_model_changed_, true);
    model_index = CancelDrag(model_index);
  }
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
  if (model_index <= last_visible_index_ ||
      model_index >= model_->FirstPanelIndex()) {
    bounds_animator_->SetAnimationDelegate(
        view, new StartFadeAnimationDelegate(this, view), true);
  } else {
    // Undo the hiding if animation does not run.
    view->layer()->SetOpacity(1.0f);
  }
}

void LauncherView::LauncherItemRemoved(int model_index, LauncherID id) {
  if (id == context_menu_id_)
    launcher_menu_runner_.reset();
  {
    base::AutoReset<bool> cancelling_drag(
        &cancelling_drag_model_changed_, true);
    model_index = CancelDrag(model_index);
  }
  views::View* view = view_model_->view_at(model_index);
  view_model_->Remove(model_index);
  // The first animation fades out the view. When done we'll animate the rest of
  // the views to their target location.
  bounds_animator_->AnimateViewTo(view, view->bounds());
  bounds_animator_->SetAnimationDelegate(
      view, new FadeOutAnimationDelegate(this, view), true);

  // If overflow bubble is visible, sanitize overflow range first and when the
  // above animation finishes, CalculateIdealBounds will be called to get
  // correct overflow range. CalculateIdealBounds could hide overflow bubble
  // and triggers LauncherItemChanged. And since we are still in the middle
  // of LauncherItemRemoved, LauncherView in overflow bubble is not synced
  // with LauncherModel and will crash.
  if (overflow_bubble_ && overflow_bubble_->IsShowing()) {
    last_hidden_index_ = std::min(last_hidden_index_,
                                  view_model_->view_size() - 1);
    UpdateOverflowRange(overflow_bubble_->launcher_view());
  }

  // Close the tooltip because it isn't needed any longer and its anchor view
  // will be deleted soon.
  if (tooltip_->GetCurrentAnchorView() == view)
    tooltip_->Close();
}

void LauncherView::LauncherItemChanged(int model_index,
                                       const ash::LauncherItem& old_item) {
  const LauncherItem& item(model_->items()[model_index]);
  if (old_item.type != item.type) {
    // Type changed, swap the views.
    model_index = CancelDrag(model_index);
    scoped_ptr<views::View> old_view(view_model_->view_at(model_index));
    bounds_animator_->StopAnimatingView(old_view.get());
    // Removing and re-inserting a view in our view model will strip the ideal
    // bounds from the item. To avoid recalculation of everything the bounds
    // get remembered and restored after the insertion to the previous value.
    gfx::Rect old_ideal_bounds = view_model_->ideal_bounds(model_index);
    view_model_->Remove(model_index);
    views::View* new_view = CreateViewForItem(item);
    AddChildView(new_view);
    view_model_->Add(new_view, model_index);
    view_model_->set_ideal_bounds(model_index, old_ideal_bounds);
    new_view->SetBoundsRect(old_view->bounds());
    return;
  }

  views::View* view = view_model_->view_at(model_index);
  switch (item.type) {
    case TYPE_BROWSER_SHORTCUT:
      // Fallthrough for the new Launcher since it needs to show the activation
      // change as well.
    case TYPE_APP_SHORTCUT:
    case TYPE_WINDOWED_APP:
    case TYPE_PLATFORM_APP:
    case TYPE_APP_PANEL: {
      LauncherButton* button = static_cast<LauncherButton*>(view);
      ReflectItemStatus(item, button);
      // The browser shortcut is currently not a "real" item and as such the
      // the image is bogous as well. We therefore keep the image as is for it.
      if (item.type != TYPE_BROWSER_SHORTCUT)
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
  // When cancelling a drag due to a launcher item being added, the currently
  // dragged item is moved back to its initial position. AnimateToIdealBounds
  // will be called again when the new item is added to the |view_model_| but
  // at this time the |view_model_| is inconsistent with the |model_|.
  if (!cancelling_drag_model_changed_)
    AnimateToIdealBounds();
}

void LauncherView::LauncherStatusChanged() {
  if (ash::switches::UseAlternateShelfLayout())
    return;
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

  int index = view_model_->GetIndexOfView(view);
  if (index == -1)
    return;

  LauncherItemDelegate* item_delegate = item_manager_->GetLauncherItemDelegate(
      model_->items()[index].type);
  if (view_model_->view_size() <= 1 ||
      !item_delegate->IsDraggable(model_->items()[index]))
    return;  // View is being deleted or not draggable, ignore request.

  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();

  drag_view_ = view;
  drag_offset_ = shelf->PrimaryAxisValue(event.x(), event.y());
}

void LauncherView::PointerDraggedOnButton(views::View* view,
                                          Pointer pointer,
                                          const ui::LocatedEvent& event) {
  ShelfLayoutManager* shelf = tooltip_->shelf_layout_manager();
  if (!dragging() && drag_view_ &&
      shelf->PrimaryAxisValue(abs(event.x() - drag_offset_),
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
    FinalizeRipOffDrag(false);
    drag_pointer_ = NONE;
    AnimateToIdealBounds();
  }
  // If the drag pointer is NONE, no drag operation is going on and the
  // drag_view can be released.
  if (drag_pointer_ == NONE)
    drag_view_ = NULL;
}

void LauncherView::MouseMovedOverButton(views::View* view) {
  if (!ShouldShowTooltipForView(view))
    return;

  if (!tooltip_->IsVisible())
    tooltip_->ResetTimer();
}

void LauncherView::MouseEnteredButton(views::View* view) {
  if (!ShouldShowTooltipForView(view))
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

base::string16 LauncherView::GetAccessibleName(const views::View* view) {
  int view_index = view_model_->GetIndexOfView(view);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return base::string16();

  LauncherItemDelegate* item_delegate = item_manager_->GetLauncherItemDelegate(
      model_->items()[view_index].type);
  return item_delegate->GetTitle(model_->items()[view_index]);
}

void LauncherView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  // Do not handle mouse release during drag.
  if (dragging())
    return;

  if (sender == overflow_button_) {
    ToggleOverflowBubble();
    return;
  }

  int view_index = view_model_->GetIndexOfView(sender);
  // May be -1 while in the process of animating closed.
  if (view_index == -1)
    return;

  // If the previous menu was closed by the same event as this one, we ignore
  // the call.
  if (!IsUsableEvent(event))
    return;

  {
    ScopedTargetRootWindow scoped_target(
        sender->GetWidget()->GetNativeView()->GetRootWindow());
    // Slow down activation animations if shift key is pressed.
    scoped_ptr<ui::ScopedAnimationDurationScaleMode> slowing_animations;
    if (event.IsShiftDown()) {
      slowing_animations.reset(new ui::ScopedAnimationDurationScaleMode(
            ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
    }

    // Collect usage statistics before we decide what to do with the click.
    switch (model_->items()[view_index].type) {
      case TYPE_APP_SHORTCUT:
      case TYPE_WINDOWED_APP:
      case TYPE_PLATFORM_APP:
      case TYPE_BROWSER_SHORTCUT:
        Shell::GetInstance()->delegate()->RecordUserMetricsAction(
            UMA_LAUNCHER_CLICK_ON_APP);
        break;

      case TYPE_APP_LIST:
        Shell::GetInstance()->delegate()->RecordUserMetricsAction(
            UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON);
        break;

      case TYPE_APP_PANEL:
        break;

      case TYPE_UNDEFINED:
        NOTREACHED() << "LauncherItemType must be set.";
        break;
    }

    LauncherItemDelegate* item_delegate =
        item_manager_->GetLauncherItemDelegate(
            model_->items()[view_index].type);
    item_delegate->ItemSelected(model_->items()[view_index], event);

    ShowListMenuForView(model_->items()[view_index], sender, event);
  }
}

bool LauncherView::ShowListMenuForView(const LauncherItem& item,
                                       views::View* source,
                                       const ui::Event& event) {
  scoped_ptr<ash::LauncherMenuModel> menu_model;
  LauncherItemDelegate* item_delegate =
      item_manager_->GetLauncherItemDelegate(item.type);
  menu_model.reset(item_delegate->CreateApplicationMenu(item, event.flags()));

  // Make sure we have a menu and it has at least two items in addition to the
  // application title and the 3 spacing separators.
  if (!menu_model.get() || menu_model->GetItemCount() <= 5)
    return false;

  ShowMenu(scoped_ptr<views::MenuModelAdapter>(
               new LauncherMenuModelAdapter(menu_model.get())),
           source,
           gfx::Point(),
           false,
           ui::GetMenuSourceTypeForEvent(event));
  return true;
}

void LauncherView::ShowContextMenuForView(views::View* source,
                                          const gfx::Point& point,
                                          ui:: MenuSourceType source_type) {
  int view_index = view_model_->GetIndexOfView(source);
  // TODO(simon.hong81): Create LauncherContextMenu for applist in its
  // LauncherItemDelegate.
  if (view_index != -1 &&
      model_->items()[view_index].type == TYPE_APP_LIST) {
    view_index = -1;
  }

  if (view_index == -1) {
    Shell::GetInstance()->ShowContextMenu(point, source_type);
    return;
  }
  scoped_ptr<ui::MenuModel> menu_model;
  LauncherItemDelegate* item_delegate = item_manager_->GetLauncherItemDelegate(
      model_->items()[view_index].type);
  menu_model.reset(item_delegate->CreateContextMenu(
      model_->items()[view_index],
      source->GetWidget()->GetNativeView()->GetRootWindow()));
  if (!menu_model)
    return;

  base::AutoReset<LauncherID> reseter(
      &context_menu_id_,
      view_index == -1 ? 0 : model_->items()[view_index].id);

  ShowMenu(scoped_ptr<views::MenuModelAdapter>(
               new views::MenuModelAdapter(menu_model.get())),
           source,
           point,
           true,
           source_type);
}

void LauncherView::ShowMenu(
    scoped_ptr<views::MenuModelAdapter> menu_model_adapter,
    views::View* source,
    const gfx::Point& click_point,
    bool context_menu,
    ui::MenuSourceType source_type) {
  closing_event_time_ = base::TimeDelta();
  launcher_menu_runner_.reset(
      new views::MenuRunner(menu_model_adapter->CreateMenu()));

  ScopedTargetRootWindow scoped_target(
      source->GetWidget()->GetNativeView()->GetRootWindow());

  // Determine the menu alignment dependent on the shelf.
  views::MenuItemView::AnchorPosition menu_alignment =
      views::MenuItemView::TOPLEFT;
  gfx::Rect anchor_point = gfx::Rect(click_point, gfx::Size());

  ShelfWidget* shelf = RootWindowController::ForLauncher(
      GetWidget()->GetNativeView())->shelf();
  if (!context_menu) {
    // Application lists use a bubble.
    ash::ShelfAlignment align = shelf->GetAlignment();
    anchor_point = source->GetBoundsInScreen();

    // It is possible to invoke the menu while it is sliding into view. To cover
    // that case, the screen coordinates are offsetted by the animation delta.
    gfx::Vector2d offset =
        source->GetWidget()->GetNativeWindow()->bounds().origin() -
        source->GetWidget()->GetNativeWindow()->GetTargetBounds().origin();
    anchor_point.set_x(anchor_point.x() - offset.x());
    anchor_point.set_y(anchor_point.y() - offset.y());

    // Launcher items can have an asymmetrical border for spacing reasons.
    // Adjust anchor location for this.
    if (source->border())
      anchor_point.Inset(source->border()->GetInsets());

    switch (align) {
      case ash::SHELF_ALIGNMENT_BOTTOM:
        menu_alignment = views::MenuItemView::BUBBLE_ABOVE;
        break;
      case ash::SHELF_ALIGNMENT_LEFT:
        menu_alignment = views::MenuItemView::BUBBLE_RIGHT;
        break;
      case ash::SHELF_ALIGNMENT_RIGHT:
        menu_alignment = views::MenuItemView::BUBBLE_LEFT;
        break;
      case ash::SHELF_ALIGNMENT_TOP:
        menu_alignment = views::MenuItemView::BUBBLE_BELOW;
        break;
    }
  }
  // If this gets deleted while we are in the menu, the launcher will be gone
  // as well.
  bool got_deleted = false;
  got_deleted_ = &got_deleted;

  shelf->ForceUndimming(true);
  // NOTE: if you convert to HAS_MNEMONICS be sure and update menu building
  // code.
  if (launcher_menu_runner_->RunMenuAt(
          source->GetWidget(),
          NULL,
          anchor_point,
          menu_alignment,
          source_type,
          context_menu ? views::MenuRunner::CONTEXT_MENU : 0) ==
      views::MenuRunner::MENU_DELETED) {
    if (!got_deleted) {
      got_deleted_ = NULL;
      shelf->ForceUndimming(false);
    }
    return;
  }
  got_deleted_ = NULL;
  shelf->ForceUndimming(false);

  // Unpinning an item will reset the |launcher_menu_runner_| before coming
  // here.
  if (launcher_menu_runner_)
    closing_event_time_ = launcher_menu_runner_->closing_event_time();
  Shell::GetInstance()->UpdateShelfVisibility();
}

void LauncherView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
  FOR_EACH_OBSERVER(LauncherIconObserver, observers_,
                    OnLauncherIconPositionsChanged());
  PreferredSizeChanged();
}

void LauncherView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  if (snap_back_from_rip_off_view_ && animator == bounds_animator_) {
    if (!animator->IsAnimating(snap_back_from_rip_off_view_)) {
      // Coming here the animation of the LauncherButton is finished and the
      // previously hidden status can be shown again. Since the button itself
      // might have gone away or changed locations we check that the button
      // is still in the shelf and show its status again.
      for (int index = 0; index < view_model_->view_size(); index++) {
        views::View* view = view_model_->view_at(index);
        if (view == snap_back_from_rip_off_view_) {
          LauncherButton* button = static_cast<LauncherButton*>(view);
          button->ClearState(LauncherButton::STATE_HIDDEN);
          break;
        }
      }
      snap_back_from_rip_off_view_ = NULL;
    }
  }
}

bool LauncherView::IsUsableEvent(const ui::Event& event) {
  if (closing_event_time_ == base::TimeDelta())
    return true;

  base::TimeDelta delta =
      base::TimeDelta(event.time_stamp() - closing_event_time_);
  closing_event_time_ = base::TimeDelta();
  // TODO(skuhne): This time seems excessive, but it appears that the reposting
  // takes that long.  Need to come up with a better way of doing this.
  return (delta.InMilliseconds() < 0 || delta.InMilliseconds() > 130);
}

const LauncherItem* LauncherView::LauncherItemForView(
    const views::View* view) const {
  int view_index = view_model_->GetIndexOfView(view);
  if (view_index == -1)
    return NULL;
  return &(model_->items()[view_index]);
}

bool LauncherView::ShouldShowTooltipForView(const views::View* view) const {
  if (view == GetAppListButtonView() &&
      Shell::GetInstance()->GetAppListWindow())
    return false;
  const LauncherItem* item = LauncherItemForView(view);
  if (!item)
    return true;
  LauncherItemDelegate* item_delegate =
      item_manager_->GetLauncherItemDelegate(item->type);
  return item_delegate->ShouldShowTooltip(*item);
}

int LauncherView::CalculateShelfDistance(const gfx::Point& coordinate) const {
  ShelfWidget* shelf = RootWindowController::ForLauncher(
      GetWidget()->GetNativeView())->shelf();
  ash::ShelfAlignment align = shelf->GetAlignment();
  const gfx::Rect bounds = GetBoundsInScreen();
  int distance = 0;
  switch (align) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
      distance = bounds.y() - coordinate.y();
      break;
    case ash::SHELF_ALIGNMENT_LEFT:
      distance = coordinate.x() - bounds.right();
      break;
    case ash::SHELF_ALIGNMENT_RIGHT:
      distance = bounds.x() - coordinate.x();
      break;
    case ash::SHELF_ALIGNMENT_TOP:
      distance = coordinate.y() - bounds.bottom();
      break;
  }
  return distance > 0 ? distance : 0;
}

}  // namespace internal
}  // namespace ash
