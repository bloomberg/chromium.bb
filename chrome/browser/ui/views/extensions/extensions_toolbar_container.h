// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class Browser;
class ExtensionsToolbarButton;
class ToolbarActionViewController;

// Container for extensions shown in the toolbar. These include pinned
// extensions and extensions that are 'popped out' transitively to show dialogs
// or be called out to the user.
// This container is used when the extension-menu experiment is active as a
// replacement for BrowserActionsContainer and ToolbarActionsBar which are
// intended to be removed.
// TODO(crbug.com/943702): Remove note related to extensions menu when cleaning
// up after the experiment.
class ExtensionsToolbarContainer : public ToolbarIconContainerView,
                                   public ExtensionsContainer,
                                   public TabStripModelObserver,
                                   public ToolbarActionsModel::Observer,
                                   public ToolbarActionView::Delegate,
                                   public views::WidgetObserver {
 public:
  using ToolbarIconMap = std::map<ToolbarActionsModel::ActionId,
                                  std::unique_ptr<ToolbarActionView>>;

  // Determines how the container displays - specifically whether the menu and
  // popped out action can be hidden.
  enum class DisplayMode {
    // In normal mode, the menu icon and popped-out action is always visible.
    // Normal mode is used for the main toolbar and in windows where there is
    // always enough space to show at least two icons.
    kNormal,
    // In compact mode, one or both of the menu icon and popped-out action may
    // be hidden. Compact mode is used in smaller windows (e.g. webapps) where
    // there may not be enough space to display the buttons.
    kCompact,
  };

  explicit ExtensionsToolbarContainer(
      Browser* browser,
      DisplayMode display_mode = DisplayMode::kNormal);
  ~ExtensionsToolbarContainer() override;

  ExtensionsToolbarButton* extensions_button() const {
    return extensions_button_;
  }
  const ToolbarIconMap& icons_for_testing() const { return icons_; }
  ToolbarActionViewController* popup_owner_for_testing() {
    return popup_owner_;
  }

  // Get the view corresponding to the extension |id|, if any.
  ToolbarActionView* GetViewForId(const std::string& id);

  // Pop out and show the extension corresponding to |extension_id|, then show
  // the Widget when the icon is visible. If the icon is already visible the
  // action will be posted immediately (not run synchronously).
  void ShowWidgetForExtension(views::Widget* widget,
                              const std::string& extension_id);

  // Gets the widget that anchors to the extension (or is about to anchor to the
  // extension, pending pop-out).
  views::Widget* GetAnchoredWidgetForExtensionForTesting(
      const std::string& extension_id);

  base::Optional<extensions::ExtensionId>
  GetExtensionWithOpenContextMenuForTesting() {
    return extension_with_open_context_menu_id_;
  }

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  const char* GetClassName() const override;

  // ExtensionsContainer:
  ToolbarActionViewController* GetActionForId(
      const std::string& action_id) override;
  ToolbarActionViewController* GetPoppedOutAction() const override;
  void OnContextMenuShown(ToolbarActionViewController* extension) override;
  void OnContextMenuClosed(ToolbarActionViewController* extension) override;
  bool IsActionVisibleOnToolbar(
      const ToolbarActionViewController* action) const override;
  extensions::ExtensionContextMenuModel::ButtonVisibility GetActionVisibility(
      const ToolbarActionViewController* action) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewController* popup_owner) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  void PopOutAction(ToolbarActionViewController* action,
                    bool is_sticky,
                    const base::Closure& closure) override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id) override;
  void ShowToolbarActionBubble(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) override;
  void ShowToolbarActionBubbleAsync(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) override;

  // ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override;
  bool ShownInsideMenu() const override;
  void OnToolbarActionViewDragDone() override;
  views::LabelButton* GetOverflowReferenceView() const override;
  gfx::Size GetToolbarActionSize() override;
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

 private:
  // A struct representing the position and action being dragged.
  struct DropInfo;

  // Pairing of widgets associated with this container and the extension they
  // are associated with. This is used to keep track of icons that are popped
  // out due to a widget showing (or being queued to show).
  struct AnchoredWidget {
    views::Widget* widget;
    std::string extension_id;
  };

  // Determines whether an action must be visible (i.e. cannot be hidden for any
  // reason). Returns true if the action is popped out or has an attached
  // bubble.
  bool ShouldForceVisibility(const std::string& extension_id) const;

  // Updates the view's visibility state according to
  // IsActionVisibleOnToolbar(). Note that IsActionVisibleOnToolbar() does not
  // return View visibility but whether the action should be visible or not
  // (according to pin and pop-out state).
  void UpdateIconVisibility(const std::string& extension_id);

  // Set |widget|'s anchor (to the corresponding extension) and then show it.
  // Posted from |ShowWidgetForExtension|.
  void AnchorAndShowWidgetImmediately(views::Widget* widget);

  // Creates toolbar actions and icons corresponding to the model. This is only
  // called in the constructor or when the model initializes and should not be
  // called for subsequent changes to the model.
  void CreateActions();

  // Creates an action and toolbar button for the corresponding ID.
  void CreateActionForId(const ToolbarActionsModel::ActionId& action_id);

  // Sorts child views to display them in the correct order (pinned actions,
  // popped out actions, extensions button).
  void ReorderViews();

  // Utility function for going from width to icon counts.
  size_t WidthToIconCount(int x_offset);

  gfx::ImageSkia GetExtensionIcon(ToolbarActionView* extension_view);

  // Sets a pinned extension button's image to be shown/hidden.
  void SetExtensionIconVisibility(ToolbarActionsModel::ActionId id,
                                  bool visible);

  // Calls SetVisible to make sure that the container is showing only when there
  // are extensions available.
  void UpdateContainerVisibility();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionMoved(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionLoadFailed() override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  Browser* const browser_;
  ToolbarActionsModel* const model_;
  ScopedObserver<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observer_;
  ExtensionsToolbarButton* const extensions_button_;
  DisplayMode display_mode_;

  // TODO(pbos): Create actions and icons only for pinned pinned / popped out
  // actions (lazily). Currently code expects GetActionForId() to return
  // actions for extensions that aren't visible.
  // Actions for all extensions.
  std::vector<std::unique_ptr<ToolbarActionViewController>> actions_;
  // View for every action, does not imply pinned or currently shown.
  ToolbarIconMap icons_;
  // Popped-out extension, if any.
  ToolbarActionViewController* popped_out_action_ = nullptr;
  // The action that triggered the current popup, if any.
  ToolbarActionViewController* popup_owner_ = nullptr;
  // Extension with an open context menu, if any.
  base::Optional<extensions::ExtensionId> extension_with_open_context_menu_id_;

  // The widgets currently popped out and, for each, the extension it is
  // associated with. See AnchoredWidget.
  std::vector<AnchoredWidget> anchored_widgets_;

  // The DropInfo for the current drag-and-drop operation, or a null pointer if
  // there is none.
  std::unique_ptr<DropInfo> drop_info_;

  base::WeakPtrFactory<ExtensionsToolbarContainer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionsToolbarContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
