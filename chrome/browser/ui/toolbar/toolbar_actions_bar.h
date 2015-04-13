// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {
class Extension;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class ToolbarActionsBarDelegate;
class ToolbarActionViewController;

// A platform-independent version of the container for toolbar actions,
// including extension actions and component actions.
// This class manages the order of the actions, the actions' state, and owns the
// action controllers, in addition to (for extensions) interfacing with the
// extension toolbar model. Further, it manages dimensions for the bar,
// excluding animations.
// This can come in two flavors, main and "overflow". The main bar is visible
// next to the omnibox, and the overflow bar is visible inside the chrome
// (fka wrench) menu. The main bar can have only a single row of icons with
// flexible width, whereas the overflow bar has multiple rows of icons with a
// fixed width (the width of the menu).
class ToolbarActionsBar : public extensions::ExtensionToolbarModel::Observer,
                          public ToolbarActionsBarBubbleDelegate {
 public:
  // A struct to contain the platform settings.
  struct PlatformSettings {
    explicit PlatformSettings(bool in_overflow_mode);

    // The padding that comes before the first icon in the container.
    int left_padding;
    // The padding following the final icon in the container.
    int right_padding;
    // The spacing between each of the icons.
    int item_spacing;
    // The number of icons per row in the overflow menu.
    int icons_per_overflow_menu_row;
    // Whether or not the overflow menu is displayed as a chevron (this is being
    // phased out).
    bool chevron_enabled;
  };

  // The type of drag that occurred in a drag-and-drop operation.
  enum DragType {
    // The icon was dragged to the same container it started in.
    DRAG_TO_SAME,
    // The icon was dragged from the main container to the overflow.
    DRAG_TO_OVERFLOW,
    // The icon was dragged from the overflow container to the main.
    DRAG_TO_MAIN,
  };

  ToolbarActionsBar(ToolbarActionsBarDelegate* delegate,
                    Browser* browser,
                    ToolbarActionsBar* main_bar);
  ~ToolbarActionsBar() override;

  // Returns the width of a browser action icon, optionally including the
  // following padding.
  static int IconWidth(bool include_padding);

  // Returns the height of a browser action icon.
  static int IconHeight();

  // Registers profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the preferred size for the toolbar; this does *not* reflect any
  // animations that may be running.
  gfx::Size GetPreferredSize() const;

  // Returns the [minimum|maximum] possible width for the toolbar.
  int GetMinimumWidth() const;
  int GetMaximumWidth() const;

  // Returns the width for the given number of icons.
  int IconCountToWidth(int icons) const;

  // Returns the number of icons that can fit within the given width.
  size_t WidthToIconCount(int width) const;

  // Returns the number of icons that should be displayed.
  size_t GetIconCount() const;

  // Creates the toolbar actions.
  void CreateActions();

  // Deletes all toolbar actions.
  void DeleteActions();

  // Updates all the toolbar actions.
  void Update();

  // Sets the width for the overflow menu rows.
  void SetOverflowRowWidth(int width);

  // Notifies the ToolbarActionsBar that a user completed a resize gesture, and
  // the new width is |width|.
  void OnResizeComplete(int width);

  // Notifies the ToolbarActionsBar that a user completed a drag and drop event,
  // and dragged the view from |dragged_index| to |dropped_index|.
  // |drag_type| indicates whether or not the icon was dragged between the
  // overflow and main containers.
  // The main container should handle all drag/drop notifications.
  void OnDragDrop(int dragged_index,
                  int dropped_index,
                  DragType drag_type);

  // Returns true if the info bubble about the toolbar redesign should be shown.
  bool ShouldShowInfoBubble();

  const std::vector<ToolbarActionViewController*>& toolbar_actions() const {
    return toolbar_actions_.get();
  }
  bool enabled() const { return model_ != nullptr; }
  bool suppress_layout() const { return suppress_layout_; }
  bool suppress_animation() const {
    return suppress_animation_ || disable_animations_for_testing_;
  }
  bool is_highlighting() const { return model_ && model_->is_highlighting(); }
  const PlatformSettings& platform_settings() const {
    return platform_settings_;
  }

  ToolbarActionsBarDelegate* delegate_for_test() { return delegate_; }

  static void set_pop_out_actions_to_run_for_testing(
      bool pop_out_actions_to_run) {
    pop_out_actions_to_run_ = pop_out_actions_to_run;
  }

  static void set_send_overflowed_action_changes_for_testing(
      bool send_overflowed_action_changes) {
    send_overflowed_action_changes_ = send_overflowed_action_changes;
  }

  // During testing we can disable animations by setting this flag to true,
  // so that the bar resizes instantly, instead of having to poll it while it
  // animates to open/closed status.
  static bool disable_animations_for_testing_;

 private:
  class TabOrderHelper;

  using ToolbarActions = ScopedVector<ToolbarActionViewController>;

  // ExtensionToolbarModel::Observer:
  void OnToolbarExtensionAdded(const extensions::Extension* extension,
                               int index) override;
  void OnToolbarExtensionRemoved(
      const extensions::Extension* extension) override;
  void OnToolbarExtensionMoved(const extensions::Extension* extension,
                               int index) override;
  void OnToolbarExtensionUpdated(
      const extensions::Extension* extension) override;
  bool ShowExtensionActionPopup(const extensions::Extension* extension,
                                bool grant_active_tab) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;
  Browser* GetBrowser() override;

  // ToolbarActionsBarBubbleDelegate:
  void OnToolbarActionsBarBubbleShown() override;
  void OnToolbarActionsBarBubbleClosed(CloseAction action) override;

  // Resizes the delegate (if necessary) to the preferred size using the given
  // |tween_type| and optionally suppressing the chevron.
  void ResizeDelegate(gfx::Tween::Type tween_type, bool suppress_chevron);

  // Returns the action for the given |id|, if one exists.
  ToolbarActionViewController* GetActionForId(const std::string& id);

  // Returns the current web contents.
  content::WebContents* GetCurrentWebContents();

  // Reorders the toolbar actions to reflect the model and, optionally, to
  // "pop out" any overflowed actions that want to run (depending on the
  // value of |pop_out_actions_to_run|.
  void ReorderActions();

  // Sets |overflowed_action_wants_to_run_| to the proper value.
  void SetOverflowedActionWantsToRun();

  bool in_overflow_mode() const { return main_bar_ != nullptr; }

  // The delegate for this object (in a real build, this is the view).
  ToolbarActionsBarDelegate* delegate_;

  // The associated browser.
  Browser* browser_;

  // The observed toolbar model.
  extensions::ExtensionToolbarModel* model_;

  // The controller for the main toolbar actions bar. This will be null if this
  // is the main bar.
  ToolbarActionsBar* main_bar_;

  // Platform-specific settings for dimensions and the overflow chevron.
  PlatformSettings platform_settings_;

  // The toolbar actions.
  ToolbarActions toolbar_actions_;

  // The TabOrderHelper that manages popping out actions that want to act.
  // This is only non-null if |pop_out_actions_to_run| is true.
  scoped_ptr<TabOrderHelper> tab_order_helper_;

  ScopedObserver<extensions::ExtensionToolbarModel,
                 extensions::ExtensionToolbarModel::Observer> model_observer_;

  // True if we should suppress layout, such as when we are creating or
  // adjusting a lot of actions at once.
  bool suppress_layout_;

  // True if we should suppress animation; we do this when first creating the
  // toolbar, and also when switching tabs changes the state of the icons.
  bool suppress_animation_;

  // If this is true, actions that want to run (e.g., an extension's page
  // action) will pop out of overflow to draw more attention.
  // See also TabOrderHelper in the .cc file.
  static bool pop_out_actions_to_run_;

  // If set to false, notifications for OnOverflowedActionWantsToRunChanged()
  // will not be sent. Used because in unit tests there is no wrench menu to
  // alter.
  static bool send_overflowed_action_changes_;

  // True if an action in the overflow menu wants to run.
  bool overflowed_action_wants_to_run_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBar);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_H_
