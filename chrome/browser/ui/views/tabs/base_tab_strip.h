// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_STRIP_H_
#pragma once

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "views/animation/bounds_animator.h"
#include "views/view.h"

class BaseTab;
class DraggedTabController;
class TabStripController;

// Base class for the view tab strip implementations.
class BaseTabStrip : public views::View,
                     public TabController {
 public:
  enum Type {
    HORIZONTAL_TAB_STRIP,
    VERTICAL_TAB_STRIP
  };

  BaseTabStrip(TabStripController* controller, Type type);
  virtual ~BaseTabStrip();

  Type type() const { return type_; }

  // Returns the preferred height of this TabStrip. This is based on the
  // typical height of its constituent tabs.
  virtual int GetPreferredHeight() = 0;

  // Set the background offset used by inactive tabs to match the frame image.
  virtual void SetBackgroundOffset(const gfx::Point& offset) = 0;

  // Returns true if the specified point(TabStrip coordinates) is
  // in the window caption area of the browser window.
  virtual bool IsPositionInWindowCaption(const gfx::Point& point) = 0;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame.
  void UpdateLoadingAnimations();

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  virtual bool IsAnimating() const;

  // Starts highlighting the tab at the specified index.
  virtual void StartHighlight(int model_index) = 0;

  // Stops all tab higlighting.
  virtual void StopAllHighlighting() = 0;

  // Returns the selected tab.
  virtual BaseTab* GetSelectedBaseTab() const;

  // Retrieves the ideal bounds for the Tab at the specified index.
  const gfx::Rect& ideal_bounds(int tab_data_index) {
    return tab_data_[tab_data_index].ideal_bounds;
  }

  // Creates and returns a tab that can be used for dragging. Ownership passes
  // to the caller.
  virtual BaseTab* CreateTabForDragging() = 0;

  // Adds a tab at the specified index.
  void AddTabAt(int model_index,
                bool foreground,
                const TabRendererData& data);

  // Invoked from the controller when the close initiates from the TabController
  // (the user clicked the tab close button or middle clicked the tab). This is
  // invoked from Close. Because of unload handlers Close is not always
  // immediately followed by RemoveTabAt.
  virtual void PrepareForCloseAt(int model_index) {}

  // Removes a tab at the specified index.
  virtual void RemoveTabAt(int model_index) = 0;

  // Selects a tab at the specified index. |old_model_index| is the selected
  // index prior to the selection change.
  virtual void SelectTabAt(int old_model_index, int new_model_index) = 0;

  // Moves a tab.
  virtual void MoveTab(int from_model_index, int to_model_index);

  // Invoked when the title of a tab changes and the tab isn't loading.
  virtual void TabTitleChangedNotLoading(int model_index) = 0;

  // Sets the tab data at the specified model index.
  virtual void SetTabData(int model_index, const TabRendererData& data);

  // Returns the tab at the specified model index.
  virtual BaseTab* GetBaseTabAtModelIndex(int model_index) const;

  // Returns the tab at the specified tab index.
  BaseTab* base_tab_at_tab_index(int tab_index) const {
    return tab_data_[tab_index].tab;
  }

  // Returns the index of the specified tab in the model coordiate system, or
  // -1 if tab is closing or not valid.
  virtual int GetModelIndexOfBaseTab(const BaseTab* tab) const;

  // Gets the number of Tabs in the tab strip.
  // WARNING: this is the number of tabs displayed by the tabstrip, which if
  // an animation is ongoing is not necessarily the same as the number of tabs
  // in the model.
  int tab_count() const { return static_cast<int>(tab_data_.size()); }

  // Cover method for TabStripController::GetCount.
  int GetModelCount() const;

  // Cover method for TabStripController::IsValidIndex.
  bool IsValidModelIndex(int model_index) const;

  // Returns the index into |tab_data_| corresponding to the index from the
  // TabStripModel, or |tab_data_.size()| if there is no tab representing
  // |model_index|.
  int ModelIndexToTabIndex(int model_index) const;

  TabStripController* controller() const { return controller_.get(); }

  // Returns true if a drag session is currently active.
  bool IsDragSessionActive() const;

  // Returns true if a tab is being dragged into this tab strip.
  bool IsActiveDropTarget() const;

  // TabController overrides:
  virtual void SelectTab(BaseTab* tab);
  virtual void CloseTab(BaseTab* tab);
  virtual void ShowContextMenu(BaseTab* tab, const gfx::Point& p);
  virtual bool IsTabSelected(const BaseTab* tab) const;
  virtual bool IsTabPinned(const BaseTab* tab) const;
  virtual bool IsTabCloseable(const BaseTab* tab) const;
  virtual void MaybeStartDrag(BaseTab* tab,
                              const views::MouseEvent& event);
  virtual void ContinueDrag(const views::MouseEvent& event);
  virtual bool EndDrag(bool canceled);
  virtual BaseTab* GetTabAt(BaseTab* tab,
                            const gfx::Point& tab_in_tab_coordinates);

  // View overrides:
  virtual void Layout();

 protected:
  // The Tabs we contain, and their last generated "good" bounds.
  struct TabData {
    BaseTab* tab;
    gfx::Rect ideal_bounds;
  };

  // View overrides.
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled);

  // Creates and returns a new tab. The caller owners the returned tab.
  virtual BaseTab* CreateTab() = 0;

  // Invoked from |AddTabAt| after the newly created tab has been inserted.
  // Subclasses should either start an animation, or layout.
  virtual void StartInsertTabAnimation(int model_index, bool foreground) = 0;

  // Invoked from |MoveTab| after |tab_data_| has been updated to animate the
  // move.
  virtual void StartMoveTabAnimation() = 0;

  // Starts the remove tab animation.
  virtual void StartRemoveTabAnimation(int model_index);

  // Starts the mini-tab animation.
  virtual void StartMiniTabAnimation();

  // Returns whether the highlight button should be highlighted after a remove.
  virtual bool ShouldHighlightCloseButtonAfterRemove() { return true; }

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke GenerateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  virtual void AnimateToIdealBounds() = 0;

  // Cleans up the Tab from the TabStrip. This is called from the tab animation
  // code and is not a general-purpose method.
  void RemoveAndDeleteTab(BaseTab* tab);

  // Resets the bounds of all non-closing tabs.
  virtual void GenerateIdealBounds() = 0;

  void set_ideal_bounds(int index, const gfx::Rect& bounds) {
    tab_data_[index].ideal_bounds = bounds;
  }

  // Returns the index into |tab_data_| corresponding to the specified tab, or
  // -1 if the tab isn't in |tab_data_|.
  int TabIndexOfTab(BaseTab* tab) const;

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  virtual void StopAnimating(bool layout) = 0;

  // Destroys the active drag controller.
  void DestroyDragController();

  // Used by DraggedTabController when the user starts or stops dragging a tab.
  void StartedDraggingTab(BaseTab* tab);
  void StoppedDraggingTab(BaseTab* tab);

  // See description above field for details.
  bool attaching_dragged_tab() const { return attaching_dragged_tab_; }

  views::BoundsAnimator& bounds_animator() { return bounds_animator_; }

  // Invoked prior to starting a new animation.
  virtual void PrepareForAnimation();

  // Creates an AnimationDelegate that resets state after a remove animation
  // completes. The caller owns the returned object.
  ui::AnimationDelegate* CreateRemoveTabDelegate(BaseTab* tab);

  // Invoked from Layout if the size changes or layout is really needed.
  virtual void DoLayout();

 private:
  class RemoveTabDelegate;

  friend class DraggedTabController;

  // See description above field for details.
  void set_attaching_dragged_tab(bool value) { attaching_dragged_tab_ = value; }

  scoped_ptr<TabStripController> controller_;

  const Type type_;

  std::vector<TabData> tab_data_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<DraggedTabController> drag_controller_;

  // If true, the insert is a result of a drag attaching the tab back to the
  // model.
  bool attaching_dragged_tab_;

  views::BoundsAnimator bounds_animator_;

  // Size we last layed out at.
  gfx::Size last_layout_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_STRIP_H_
