// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_VIEW_H_
#define ASH_SHELF_SHELF_VIEW_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf_button_pressed_metric_tracker.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/views/app_list_drag_and_drop_host.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ui {
class MenuModel;
}

namespace views {
class BoundsAnimator;
class MenuModelAdapter;
class MenuRunner;
}

namespace ash {
class AppListButton;
class DragImageView;
class OverflowBubble;
class OverflowButton;
class ScopedRootWindowForNewWindows;
class Shelf;
class ShelfButton;
class ShelfModel;
struct ShelfItem;
class ShelfWidget;

enum ShelfAlignmentUmaEnumValue {
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT,
  // Must be last entry in enum.
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT,
};

// ShelfView contains the shelf items visible within an active user session.
// ShelfView and LoginShelfView should never be shown together.
class ASH_EXPORT ShelfView : public views::View,
                             public ShelfModelObserver,
                             public InkDropButtonListener,
                             public views::ContextMenuController,
                             public views::FocusTraversable,
                             public views::BoundsAnimatorObserver,
                             public app_list::ApplicationDragAndDropHost,
                             public TabletModeObserver {
 public:
  ShelfView(ShelfModel* model, Shelf* shelf, ShelfWidget* shelf_widget);
  ~ShelfView() override;

  Shelf* shelf() const { return shelf_; }
  ShelfModel* model() const { return model_; }

  void Init();

  void OnShelfAlignmentChanged();

  // Returns the ideal bounds of the specified item, or an empty rect if id
  // isn't know. If the item is in an overflow shelf, the overflow icon location
  // will be returned.
  gfx::Rect GetIdealBoundsOfItemIcon(const ShelfID& id);

  // Repositions the icon for the specified item by the midpoint of the window.
  void UpdatePanelIconPosition(const ShelfID& id, const gfx::Point& midpoint);

  // Returns true if we're showing a menu.
  bool IsShowingMenu() const;

  // Returns true if overflow bubble is shown.
  bool IsShowingOverflowBubble() const;

  // Sets owner overflow bubble instance from which this shelf view pops
  // out as overflow.
  void set_owner_overflow_bubble(OverflowBubble* owner) {
    owner_overflow_bubble_ = owner;
  }

  AppListButton* GetAppListButton() const;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Returns true if the mouse cursor exits the area for launcher tooltip.
  // There are thin gaps between launcher buttons but the tooltip shouldn't hide
  // in the gaps, but the tooltip should hide if the mouse moved totally outside
  // of the buttons area.
  bool ShouldHideTooltip(const gfx::Point& cursor_location) const;

  // Returns true if a tooltip should be shown for the shelf item |view|.
  bool ShouldShowTooltipForView(const views::View* view) const;

  // Returns the title of the shelf item |view|.
  base::string16 GetTitleForView(const views::View* view) const;

  // Returns rectangle bounding all visible launcher items. Used screen
  // coordinate system.
  gfx::Rect GetVisibleItemsBoundsInScreen();

  // InkDropButtonListener:
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // Overridden from FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  FocusTraversable* GetFocusTraversableParent() override;
  View* GetFocusTraversableParentView() override;

  // Overridden from app_list::ApplicationDragAndDropHost:
  void CreateDragIconProxy(const gfx::Point& location_in_screen_coordinates,
                           const gfx::ImageSkia& icon,
                           views::View* replaced_view,
                           const gfx::Vector2d& cursor_offset_from_center,
                           float scale_factor) override;

  void CreateDragIconProxyByLocationWithNoAnimation(
      const gfx::Point& origin_in_screen_coordinates,
      const gfx::ImageSkia& icon,
      views::View* replaced_view,
      float scale_factor) override;

  void UpdateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates) override;

  void UpdateDragIconProxyByLocation(
      const gfx::Point& origin_in_screen_coordinates) override;

  void DestroyDragIconProxy() override;
  bool StartDrag(const std::string& app_id,
                 const gfx::Point& location_in_screen_coordinates) override;
  bool Drag(const gfx::Point& location_in_screen_coordinates) override;
  void EndDrag(bool cancel) override;

  // Returns true if |event| on the shelf item is going to activate the item.
  // Used to determine whether a pending ink drop should be shown or not.
  bool ShouldEventActivateButton(views::View* view, const ui::Event& event);

  // The shelf buttons use the Pointer interface to enable item reordering.
  enum Pointer { NONE, DRAG_AND_DROP, MOUSE, TOUCH };
  void PointerPressedOnButton(views::View* view,
                              Pointer pointer,
                              const ui::LocatedEvent& event);
  void PointerDraggedOnButton(views::View* view,
                              Pointer pointer,
                              const ui::LocatedEvent& event);
  void PointerReleasedOnButton(views::View* view,
                               Pointer pointer,
                               bool canceled);

  // Updates the background for the shelf items.
  void UpdateShelfItemBackground(SkColor color);

  // True if the current |drag_view_| is the given |drag_view|.
  bool IsDraggedView(const ShelfButton* drag_view) const;

  // Return the view model for test purposes.
  const views::ViewModel* view_model_for_test() const {
    return view_model_.get();
  }

  // Return the main shelf. This will return nullptr if this is not called on
  // the overflow shelf.
  ShelfView* main_shelf() { return main_shelf_; }

  const ShelfButton* drag_view() const { return drag_view_; }

  // Returns the AppListButton's current animation value, or 0.0 if the
  // animation is not running. Used to synchronize AppListButton and ShelfView's
  // icons' animations.
  double GetAppListButtonAnimationCurrentValue();

  bool is_tablet_mode_animation_running() const {
    return is_tablet_mode_animation_running_;
  }

 private:
  friend class ShelfViewTestAPI;

  class FadeOutAnimationDelegate;
  class StartFadeAnimationDelegate;

  enum RemovableState {
    REMOVABLE,      // Item can be removed when dragged away.
    DRAGGABLE,      // Item can be dragged, but will snap always back to origin.
    NOT_REMOVABLE,  // Item is fixed and can never be removed.
  };

  // Minimum distance before drag starts.
  static const int kMinimumDragDistance;

  // Returns true when this ShelfView is used for Overflow Bubble.
  // In this mode, it does not show app list, panel and overflow button.
  // Note:
  //   * When Shelf can contain only one item (overflow button) due to very
  //     small resolution screen, overflow bubble can show app list and panel
  //     button.
  bool is_overflow_mode() const { return overflow_mode_; }

  bool dragging() const { return drag_pointer_ != NONE; }

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  // Update all button's visibility in overflow.
  void UpdateAllButtonsVisibilityInOverflowMode();

  // Calculates the ideal bounds. The bounds of each button corresponding to an
  // item in the model is set in |view_model_|.
  void CalculateIdealBounds(gfx::Rect* overflow_bounds) const;

  // Returns the index of the last view whose max primary axis coordinate is
  // less than |max_value|. Returns -1 if nothing fits, or there are no views.
  int DetermineLastVisibleIndex(int max_value) const;

  // Returns the index of the first panel whose min primary axis coordinate is
  // at least |min_value|. Returns the index past the last panel if none fit.
  int DetermineFirstVisiblePanelIndex(int min_value) const;

  // Animates the bounds of each view to its ideal bounds.
  void AnimateToIdealBounds();

  // Creates the view used to represent |item|.
  views::View* CreateViewForItem(const ShelfItem& item);

  // Fades |view| from an opacity of 0 to 1. This is when adding a new item.
  void FadeIn(views::View* view);

  // Invoked when the pointer has moved enough to trigger a drag. Sets
  // internal state in preparation for the drag.
  void PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event);

  // Invoked when the mouse is dragged. Updates the models as appropriate.
  void ContinueDrag(const ui::LocatedEvent& event);

  // Ends the drag on the other shelf. (ie if we are on main shelf, ends drag on
  // the overflow shelf). Invoked when a shelf item is being dragged from one
  // shelf to the other.
  void EndDragOnOtherShelf(bool cancel);

  // Handles ripping off an item from the shelf. Returns true when the item got
  // removed.
  bool HandleRipOffDrag(const ui::LocatedEvent& event);

  // Finalize the rip off dragging by either |cancel| the action or validating.
  void FinalizeRipOffDrag(bool cancel);

  // Check if an item can be ripped off or not.
  RemovableState RemovableByRipOff(int index) const;

  // Returns true if |typea| and |typeb| should be in the same drag range.
  bool SameDragType(ShelfItemType typea, ShelfItemType typeb) const;

  // Returns the range (in the model) the item at the specified index can be
  // dragged to.
  std::pair<int, int> GetDragRange(int index);

  // If there is a drag operation in progress it's canceled. If |modified_index|
  // is valid, the new position of the corresponding item is returned.
  int CancelDrag(int modified_index);

  // Returns rectangle bounds used for drag insertion.
  // Note:
  //  * When overflow button is visible, returns bounds from first item
  //    to overflow button.
  //  * When overflow button is visible and one or more panel items exists,
  //    returns bounds from first item to last panel item.
  //  * In the overflow mode, returns only bubble's bounds.
  gfx::Rect GetBoundsForDragInsertInScreen();

  // Common setup done for all children.
  void ConfigureChildView(views::View* view);

  // Toggles the overflow menu.
  void ToggleOverflowBubble();

  // Invoked after the fading out animation for item deletion is ended.
  void OnFadeOutAnimationEnded();

  // Fade in last visible item.
  void StartFadeInLastVisibleItem();

  // Updates the visible range of overflow items in |overflow_view|.
  void UpdateOverflowRange(ShelfView* overflow_view) const;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // Overridden from ShelfModelObserver:
  void ShelfItemAdded(int model_index) override;
  void ShelfItemRemoved(int model_index, const ShelfItem& old_item) override;
  void ShelfItemChanged(int model_index, const ShelfItem& old_item) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemDelegateChanged(const ShelfID& id,
                                ShelfItemDelegate* old_delegate,
                                ShelfItemDelegate* delegate) override;

  // Handles the result when querying ShelfItemDelegates for context menu items.
  // Shows a default shelf context menu with optional extra custom |menu_items|.
  void AfterGetContextMenuItems(const ShelfID& shelf_id,
                                const gfx::Point& point,
                                views::View* source,
                                ui::MenuSourceType source_type,
                                std::vector<mojom::MenuItemPtr> menu_items);

  // Handles the result of an item selection, records the |action| taken and
  // optionally shows an application menu with the given |menu_items|.
  void AfterItemSelected(
      const ShelfItem& item,
      views::Button* sender,
      std::unique_ptr<ui::Event> event,
      views::InkDrop* ink_drop,
      ShelfAction action,
      base::Optional<std::vector<mojom::MenuItemPtr>> menu_items);

  // Show a list of all running items for this shelf |item|; it only shows a
  // menu if there are multiple running items. |source| specifies the view
  // responsible for showing the menu, and the bubble will point towards it.
  // The |event_flags| are the flags of the event which triggered this menu.
  // Returns |true| if a menu is shown.
  bool ShowListMenuForView(const ShelfItem& item,
                           views::View* source,
                           const ui::Event& event,
                           views::InkDrop* ink_drop);

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // Show either a context or normal click menu of given |menu_model|.
  // If |context_menu| is set, the displayed menu is a context menu and not
  // a menu listing one or more running applications.
  // The |click_point| is only used for |context_menu|'s.
  void ShowMenu(std::unique_ptr<ui::MenuModel> menu_model,
                views::View* source,
                const gfx::Point& click_point,
                bool context_menu,
                ui::MenuSourceType source_type,
                views::InkDrop* ink_drop);

  // Callback for MenuModelAdapter.
  void OnMenuClosed(views::InkDrop* ink_drop);

  // Overridden from views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override;
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

  // Returns true if the (press down) |event| is a repost event from an event
  // which just closed the menu of a shelf item. If it occurs on the same shelf
  // item, we should ignore the call.
  bool IsRepostEvent(const ui::Event& event);

  // Convenience accessor to model_->items().
  const ShelfItem* ShelfItemForView(const views::View* view) const;

  // Get the distance from the given |coordinate| to the closest point on this
  // launcher/shelf.
  int CalculateShelfDistance(const gfx::Point& coordinate) const;

  bool CanPrepareForDrag(Pointer pointer, const ui::LocatedEvent& event);

  // The model; owned by Launcher.
  ShelfModel* model_;

  // The shelf controller; owned by RootWindowController.
  Shelf* shelf_;

  // The shelf widget for this view. For overflow bubbles, this is the widget
  // for the shelf, not for the bubble.
  ShelfWidget* shelf_widget_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  std::unique_ptr<views::ViewModel> view_model_;

  // Index of first visible launcher item.
  int first_visible_index_ = 0;

  // Last index of a launcher button that is visible
  // (does not go into overflow).
  mutable int last_visible_index_ = -1;

  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  OverflowButton* overflow_button_ = nullptr;

  std::unique_ptr<OverflowBubble> overflow_bubble_;

  OverflowBubble* owner_overflow_bubble_ = nullptr;

  ShelfTooltipManager tooltip_;

  // Pointer device that initiated the current drag operation. If there is no
  // current dragging operation, this is NONE.
  Pointer drag_pointer_ = NONE;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  ShelfButton* drag_view_ = nullptr;

  // Position of the mouse down event in |drag_view_|'s coordinates.
  gfx::Point drag_origin_;

  // Index |drag_view_| was initially at.
  int start_drag_index_ = -1;

  // Used for the context menu of a particular item.
  ShelfID context_menu_id_;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // Manages the context menu, and the list menu.
  std::unique_ptr<ui::MenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> launcher_menu_runner_;
  std::unique_ptr<ScopedRootWindowForNewWindows>
      scoped_root_window_for_new_windows_;

  // True when an item being inserted or removed in the model cancels a drag.
  bool cancelling_drag_model_changed_ = false;

  // Index of the last hidden launcher item. If there are no hidden items this
  // will be equal to last_visible_index_ + 1.
  mutable int last_hidden_index_ = 0;

  // The timestamp of the event which closed the last menu - or 0.
  base::TimeTicks closing_event_time_;

  // True if a drag and drop operation created/pinned the item in the launcher
  // and it needs to be deleted/unpinned again if the operation gets cancelled.
  bool drag_and_drop_item_pinned_ = false;

  // The ShelfItem currently used for drag and drop; empty if none.
  ShelfID drag_and_drop_shelf_id_;

  // The original launcher item's size before the dragging operation.
  gfx::Size pre_drag_and_drop_size_;

  // The image proxy for drag operations when a drag and drop host exists and
  // the item can be dragged outside the app grid.
  std::unique_ptr<ash::DragImageView> drag_image_;

  // The cursor offset to the middle of the dragged item.
  gfx::Vector2d drag_image_offset_;

  // The view which gets replaced by our drag icon proxy.
  views::View* drag_replaced_view_ = nullptr;

  // True when the icon was dragged off the shelf.
  bool dragged_off_shelf_ = false;

  // True when an item is dragged from one shelf to another (eg. overflow).
  bool dragged_to_another_shelf_ = false;

  // The rip off view when a snap back operation is underway.
  views::View* snap_back_from_rip_off_view_ = nullptr;

  // True when this ShelfView is used for Overflow Bubble.
  bool overflow_mode_ = false;

  // Holds a pointer to main ShelfView when a ShelfView is in overflow mode.
  ShelfView* main_shelf_ = nullptr;

  // True when ripped item from overflow bubble is entered into Shelf.
  bool dragged_off_from_overflow_to_shelf_ = false;

  // True if the event is a repost event from a event which has just closed the
  // menu of the same shelf item.
  bool is_repost_event_on_same_item_ = false;

  // Record the index for the last pressed shelf item. This variable is used to
  // check if a repost event occurs on the same shelf item as previous one. If
  // so, the repost event should be ignored.
  int last_pressed_index_ = -1;

  // True while the animation to enter or exit tablet mode is running. Sometimes
  // this value is true when the shelf movements are not actually animating
  // (animation value = 0.0). This is because this is set when we enter/exit
  // tablet mode this is set to true but the animation is not started until a
  // shelf OnBoundsChanged is called because of tablet mode. Use this value to
  // sync up the animation for AppListButton.
  bool is_tablet_mode_animation_running_ = false;

  // Tracks UMA metrics based on shelf button press actions.
  ShelfButtonPressedMetricTracker shelf_button_pressed_metric_tracker_;

  base::WeakPtrFactory<ShelfView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_H_
