// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_VIEW_H_
#define ASH_SHELF_SHELF_VIEW_H_

#include <utility>
#include <vector>

#include "ash/shelf/shelf_button_host.h"
#include "ash/shelf/shelf_model_observer.h"
#include "ash/wm/gestures/shelf_gesture_handler.h"
#include "base/observer_list.h"
#include "ui/app_list/views/app_list_drag_and_drop_host.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace ui {
class MenuModel;
}

namespace views {
class BoundsAnimator;
class MenuRunner;
class ViewModel;
}

namespace ash {
class ShelfDelegate;
class ShelfIconObserver;
class ShelfItemDelegateManager;
class ShelfModel;
struct ShelfItem;
class DragImageView;
class OverflowBubble;
class OverflowButton;
class ShelfButton;
class ShelfLayoutManager;
class ShelfTooltipManager;

namespace test {
class ShelfViewTestAPI;
}

extern const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM;
extern const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT;
extern const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT;
extern const int SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT;

class ASH_EXPORT ShelfView : public views::View,
                             public ShelfModelObserver,
                             public views::ButtonListener,
                             public ShelfButtonHost,
                             public views::ContextMenuController,
                             public views::FocusTraversable,
                             public views::BoundsAnimatorObserver,
                             public app_list::ApplicationDragAndDropHost {
 public:
  ShelfView(ShelfModel* model,
            ShelfDelegate* delegate,
            ShelfLayoutManager* manager);
  virtual ~ShelfView();

  ShelfTooltipManager* tooltip_manager() { return tooltip_.get(); }

  ShelfLayoutManager* shelf_layout_manager() { return layout_manager_; }

  ShelfModel* model() { return model_; }

  void Init();

  void OnShelfAlignmentChanged();
  void SchedulePaintForAllButtons();

  // Returns the ideal bounds of the specified item, or an empty rect if id
  // isn't know. If the item is in an overflow shelf, the overflow icon location
  // will be returned.
  gfx::Rect GetIdealBoundsOfItemIcon(ShelfID id);

  // Repositions the icon for the specified item by the midpoint of the window.
  void UpdatePanelIconPosition(ShelfID id, const gfx::Point& midpoint);

  void AddIconObserver(ShelfIconObserver* observer);
  void RemoveIconObserver(ShelfIconObserver* observer);

  // Returns true if we're showing a menu.
  bool IsShowingMenu() const;

  // Returns true if overflow bubble is shown.
  bool IsShowingOverflowBubble() const;

  // Sets owner overflow bubble instance from which this shelf view pops
  // out as overflow.
  void set_owner_overflow_bubble(OverflowBubble* owner) {
    owner_overflow_bubble_ = owner;
  }

  views::View* GetAppListButtonView() const;

  // Returns true if the mouse cursor exits the area for launcher tooltip.
  // There are thin gaps between launcher buttons but the tooltip shouldn't hide
  // in the gaps, but the tooltip should hide if the mouse moved totally outside
  // of the buttons area.
  bool ShouldHideTooltip(const gfx::Point& cursor_location);

  // Returns rectangle bounding all visible launcher items. Used screen
  // coordinate system.
  gfx::Rect GetVisibleItemsBoundsInScreen();

  // Overridden from FocusTraversable:
  virtual views::FocusSearch* GetFocusSearch() OVERRIDE;
  virtual FocusTraversable* GetFocusTraversableParent() OVERRIDE;
  virtual View* GetFocusTraversableParentView() OVERRIDE;

  // Overridden from app_list::ApplicationDragAndDropHost:
  virtual void CreateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates,
      const gfx::ImageSkia& icon,
      views::View* replaced_view,
      const gfx::Vector2d& cursor_offset_from_center,
      float scale_factor) OVERRIDE;
  virtual void UpdateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates) OVERRIDE;
  virtual void DestroyDragIconProxy() OVERRIDE;
  virtual bool StartDrag(
      const std::string& app_id,
      const gfx::Point& location_in_screen_coordinates) OVERRIDE;
  virtual bool Drag(const gfx::Point& location_in_screen_coordinates) OVERRIDE;
  virtual void EndDrag(bool cancel) OVERRIDE;

  // Return the view model for test purposes.
  const views::ViewModel* view_model_for_test() const {
    return view_model_.get();
  }

 private:
  friend class ash::test::ShelfViewTestAPI;

  class FadeOutAnimationDelegate;
  class StartFadeAnimationDelegate;

  struct IdealBounds {
    gfx::Rect overflow_bounds;
  };

  enum RemovableState {
    REMOVABLE,     // Item can be removed when dragged away.
    DRAGGABLE,     // Item can be dragged, but will snap always back to origin.
    NOT_REMOVABLE, // Item is fixed and can never be removed.
  };

  // Returns true when this ShelfView is used for Overflow Bubble.
  // In this mode, it does not show app list, panel and overflow button.
  // Note:
  //   * When Shelf can contain only one item (overflow button) due to very
  //     small resolution screen, overflow bubble can show app list and panel
  //     button.
  bool is_overflow_mode() const { return overflow_mode_; }

  bool dragging() const {
    return drag_pointer_ != NONE;
  }

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  // Update all button's visibility in overflow.
  void UpdateAllButtonsVisibilityInOverflowMode();

  // Calculates the ideal bounds. The bounds of each button corresponding to an
  // item in the model is set in |view_model_|.
  void CalculateIdealBounds(IdealBounds* bounds) const;

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
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual FocusTraversable* GetPaneFocusTraversable() OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from ShelfModelObserver:
  virtual void ShelfItemAdded(int model_index) OVERRIDE;
  virtual void ShelfItemRemoved(int model_index, ShelfID id) OVERRIDE;
  virtual void ShelfItemChanged(int model_index,
                                const ShelfItem& old_item) OVERRIDE;
  virtual void ShelfItemMoved(int start_index, int target_index) OVERRIDE;
  virtual void ShelfStatusChanged() OVERRIDE;

  // Overridden from ShelfButtonHost:
  virtual void PointerPressedOnButton(views::View* view,
                                      Pointer pointer,
                                      const ui::LocatedEvent& event) OVERRIDE;
  virtual void PointerDraggedOnButton(views::View* view,
                                      Pointer pointer,
                                      const ui::LocatedEvent& event) OVERRIDE;
  virtual void PointerReleasedOnButton(views::View* view,
                                       Pointer pointer,
                                       bool canceled) OVERRIDE;
  virtual void MouseMovedOverButton(views::View* view) OVERRIDE;
  virtual void MouseEnteredButton(views::View* view) OVERRIDE;
  virtual void MouseExitedButton(views::View* view) OVERRIDE;
  virtual base::string16 GetAccessibleName(const views::View* view) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Show the list of all running items for this |item|. It will return true
  // when the menu was shown and false if there were no possible items to
  // choose from. |source| specifies the view which is responsible for showing
  // the menu, and the bubble will point towards it.
  // The |event_flags| are the flags of the event which triggered this menu.
  bool ShowListMenuForView(const ShelfItem& item,
                           views::View* source,
                           const ui::Event& event);

  // Overridden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // Show either a context or normal click menu of given |menu_model|.
  // If |context_menu| is set, the displayed menu is a context menu and not
  // a menu listing one or more running applications.
  // The |click_point| is only used for |context_menu|'s.
  void ShowMenu(ui::MenuModel* menu_model,
                views::View* source,
                const gfx::Point& click_point,
                bool context_menu,
                ui::MenuSourceType source_type);

  // Overridden from views::BoundsAnimatorObserver:
  virtual void OnBoundsAnimatorProgressed(
      views::BoundsAnimator* animator) OVERRIDE;
  virtual void OnBoundsAnimatorDone(views::BoundsAnimator* animator) OVERRIDE;

  // Returns false if the click which closed the previous menu is the click
  // which triggered this event.
  bool IsUsableEvent(const ui::Event& event);

  // Convenience accessor to model_->items().
  const ShelfItem* ShelfItemForView(const views::View* view) const;

  // Returns true if a tooltip should be shown for |view|.
  bool ShouldShowTooltipForView(const views::View* view) const;

  // Get the distance from the given |coordinate| to the closest point on this
  // launcher/shelf.
  int CalculateShelfDistance(const gfx::Point& coordinate) const;

  // The model; owned by Launcher.
  ShelfModel* model_;

  // Delegate; owned by Launcher.
  ShelfDelegate* delegate_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  scoped_ptr<views::ViewModel> view_model_;

  // Index of first visible launcher item.
  int first_visible_index_;

  // Last index of a launcher button that is visible
  // (does not go into overflow).
  mutable int last_visible_index_;

  scoped_ptr<views::BoundsAnimator> bounds_animator_;

  OverflowButton* overflow_button_;

  scoped_ptr<OverflowBubble> overflow_bubble_;

  OverflowBubble* owner_overflow_bubble_;

  scoped_ptr<ShelfTooltipManager> tooltip_;

  // Pointer device that initiated the current drag operation. If there is no
  // current dragging operation, this is NONE.
  Pointer drag_pointer_;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  views::View* drag_view_;

  // Position of the mouse down event in |drag_view_|'s coordinates.
  gfx::Point drag_origin_;

  // Index |drag_view_| was initially at.
  int start_drag_index_;

  // Used for the context menu of a particular item.
  ShelfID context_menu_id_;

  scoped_ptr<views::FocusSearch> focus_search_;

  scoped_ptr<ui::MenuModel> context_menu_model_;

  scoped_ptr<views::MenuRunner> launcher_menu_runner_;

  ObserverList<ShelfIconObserver> observers_;

  // Amount content is inset on the left edge (or top edge for vertical
  // alignment).
  int leading_inset_;

  ShelfGestureHandler gesture_handler_;

  // True when an item being inserted or removed in the model cancels a drag.
  bool cancelling_drag_model_changed_;

  // Index of the last hidden launcher item. If there are no hidden items this
  // will be equal to last_visible_index_ + 1.
  mutable int last_hidden_index_;

  // The timestamp of the event which closed the last menu - or 0.
  base::TimeDelta closing_event_time_;

  // When this object gets deleted while a menu is shown, this pointed
  // element will be set to false.
  bool* got_deleted_;

  // True if a drag and drop operation created/pinned the item in the launcher
  // and it needs to be deleted/unpinned again if the operation gets cancelled.
  bool drag_and_drop_item_pinned_;

  // The ShelfItem which is currently used for a drag and a drop operation
  // or 0 otherwise.
  ShelfID drag_and_drop_shelf_id_;

  // The application ID of the application which we drag and drop.
  std::string drag_and_drop_app_id_;

  // The original launcher item's size before the dragging operation.
  gfx::Size pre_drag_and_drop_size_;

  // The image proxy for drag operations when a drag and drop host exists and
  // the item can be dragged outside the app grid.
  scoped_ptr<ash::DragImageView> drag_image_;

  // The cursor offset to the middle of the dragged item.
  gfx::Vector2d drag_image_offset_;

  // The view which gets replaced by our drag icon proxy.
  views::View* drag_replaced_view_;

  // True when the icon was dragged off the shelf.
  bool dragged_off_shelf_;

  // The rip off view when a snap back operation is underway.
  views::View* snap_back_from_rip_off_view_;

  // Holds ShelfItemDelegateManager.
  ShelfItemDelegateManager* item_manager_;

  // Holds ShelfLayoutManager.
  ShelfLayoutManager* layout_manager_;

  // True when this ShelfView is used for Overflow Bubble.
  bool overflow_mode_;

  // Holds a pointer to main ShelfView when a ShelfView is in overflow mode.
  ShelfView* main_shelf_;

  // True when ripped item from overflow bubble is entered into Shelf.
  bool dragged_off_from_overflow_to_shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_H_
