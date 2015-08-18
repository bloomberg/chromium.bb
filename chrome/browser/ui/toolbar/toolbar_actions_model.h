// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Browser;
class PrefService;
class Profile;
class ToolbarActionsBar;
class ToolbarActionViewController;

namespace extensions {
class ExtensionRegistry;
class ExtensionSet;
}

// Model for the browser actions toolbar.
class ToolbarActionsModel : public extensions::ExtensionActionAPI::Observer,
                            public extensions::ExtensionRegistryObserver,
                            public KeyedService {
 public:
  // The different options for highlighting.
  enum HighlightType {
    HIGHLIGHT_NONE,
    HIGHLIGHT_INFO,
    HIGHLIGHT_WARNING,
  };

  // The different types of actions.
  enum ActionType {
    COMPONENT_ACTION,
    EXTENSION_ACTION,
  };

  // An action id and its corresponding ActionType.
  struct ToolbarItem {
    ToolbarItem() {}
    ToolbarItem(std::string action_id, ActionType action_type)
        : id(action_id), type(action_type) {}

    bool operator==(const ToolbarItem& other) { return other.id == id; }

    std::string id;
    ActionType type;
  };

  ToolbarActionsModel(Profile* profile,
                      extensions::ExtensionPrefs* extension_prefs);
  ~ToolbarActionsModel() override;

  // A class which is informed of changes to the model; represents the view of
  // MVC. Also used for signaling view changes such as showing extension popups.
  // TODO(devlin): Should this really be an observer? It acts more like a
  // delegate.
  class Observer {
   public:
    // Signals that an action with |id| has been added to the toolbar at
    // |index|. This will *only* be called after the toolbar model has been
    // initialized.
    virtual void OnToolbarActionAdded(const std::string& id, int index) = 0;

    // Signals that the given action with |id| has been removed from the
    // toolbar.
    virtual void OnToolbarActionRemoved(const std::string& id) = 0;

    // Signals that the given action with |id| has been moved to |index|.
    // |index| is the desired *final* index of the action (that is, in the
    // adjusted order, action should be at |index|).
    virtual void OnToolbarActionMoved(const std::string& id, int index) = 0;

    // Signals that the browser action with |id| has been updated.
    virtual void OnToolbarActionUpdated(const std::string& id) = 0;

    // Signals the action with |id| to show the popup now in the active
    // window. If |grant_active_tab| is true, then active tab permissions
    // should be given to the action (only do this if this is through a user
    // action). Returns true if a popup was slated to be shown.
    virtual bool ShowToolbarActionPopup(const std::string& id,
                                        bool grant_active_tab) = 0;

    // Signals when the container needs to be redrawn because of a size change,
    // and when the model has finished loading.
    virtual void OnToolbarVisibleCountChanged() = 0;

    // Signals that the model has entered or exited highlighting mode, or that
    // the actions being highlighted have (probably*) changed. Highlighting
    // mode indicates that only a subset of the toolbar actions are actively
    // displayed, and those actions should be highlighted for extra emphasis.
    // * probably, because if we are in highlight mode and receive a call to
    //   highlight a new set of actions, we do not compare the current set with
    //   the new set (and just assume the new set is different).
    virtual void OnToolbarHighlightModeChanged(bool is_highlighting) = 0;

    // Signals that the toolbar model has been initialized, so that if any
    // observers were postponing animation during the initialization stage, they
    // can catch up.
    virtual void OnToolbarModelInitialized() = 0;

    // Returns the browser associated with the Observer.
    virtual Browser* GetBrowser() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Convenience function to get the ToolbarActionsModel for a Profile.
  static ToolbarActionsModel* Get(Profile* profile);

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Moves the given action with |id|'s icon to the given |index|.
  void MoveActionIcon(const std::string& id, size_t index);

  // Sets the number of action icons that should be visible.
  // If count == size(), this will set the visible icon count to -1, meaning
  // "show all actions".
  void SetVisibleIconCount(size_t count);

  size_t visible_icon_count() const {
    // We have guards around this because |visible_icon_count_| can be set by
    // prefs/sync, and we want to ensure that the icon count returned is within
    // bounds.
    return visible_icon_count_ == -1
               ? toolbar_items().size()
               : std::min(static_cast<size_t>(visible_icon_count_),
                          toolbar_items().size());
  }

  bool all_icons_visible() const { return visible_icon_count_ == -1; }

  bool actions_initialized() const { return actions_initialized_; }

  ScopedVector<ToolbarActionViewController> CreateActions(
      Browser* browser,
      ToolbarActionsBar* bar);

  const std::vector<ToolbarItem>& toolbar_items() const {
    return is_highlighting() ? highlighted_items_ : toolbar_items_;
  }

  bool is_highlighting() const { return highlight_type_ != HIGHLIGHT_NONE; }
  HighlightType highlight_type() const { return highlight_type_; }

  void OnActionToolbarPrefChange();

  // Returns the index of the given action with |id|, or -1 if the id
  // wasn't found.
  int GetIndexForId(const std::string& id) const;

  // Finds the Observer associated with |browser| and tells it to display a
  // popup for the given action with |id|. If |grant_active_tab| is true,
  // this grants active tab permissions to the action; only do this because of
  // a direct user action.
  bool ShowToolbarActionPopup(const std::string& id,
                              Browser* browser,
                              bool grant_active_tab);

  // Ensures that the actions in the |action_ids| list are visible on the
  // toolbar. This might mean they need to be moved to the front (if they are in
  // the overflow bucket).
  void EnsureVisibility(const std::vector<std::string>& action_ids);

  // Highlights the actions specified by |action_ids|. This will cause
  // the ToolbarModel to only display those actions.
  // Highlighting mode is only entered if there is at least one action to be
  // shown.
  // Returns true if highlighting mode is entered, false otherwise.
  bool HighlightActions(const std::vector<std::string>& action_ids,
                        HighlightType type);

  // Stop highlighting actions. All actions can be shown again, and the
  // number of visible icons will be reset to what it was before highlighting.
  void StopHighlighting();

  // Returns true if the toolbar model is running with the redesign and is
  // showing new icons as a result.
  bool RedesignIsShowingNewIcons() const;

 private:
  // Callback when actions are ready.
  void OnReady();

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // ExtensionActionAPI::Observer:
  void OnExtensionActionUpdated(
      ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;
  void OnExtensionActionVisibilityChanged(const std::string& extension_id,
                                          bool is_now_visible) override;

  // To be called after the extension service is ready; gets loaded extensions
  // from the ExtensionRegistry and their saved order from the pref service
  // and constructs |toolbar_items_| from these data. IncognitoPopulate()
  // takes the shortcut - looking at the regular model's content and modifying
  // it.
  void InitializeActionList();
  void Populate(std::vector<std::string>* positions);
  void IncognitoPopulate();

  // Save the model to prefs.
  void UpdatePrefs();

  // Updates action with |action|'s id's browser action visibility pref if the
  // browser action is in the overflow menu and should be considered hidden.
  void MaybeUpdateVisibilityPref(const ToolbarItem& action, size_t index);

  // Calls MaybeUpdateVisibilityPref() for each action in |toolbar_items|.
  void MaybeUpdateVisibilityPrefs();

  // Finds the last known visible position of the icon for |action|. The value
  // returned is a zero-based index into the vector of visible items.
  size_t FindNewPositionFromLastKnownGood(const ToolbarItem& action);

  // Returns true if the given |extension| should be added to the toolbar.
  bool ShouldAddExtension(const extensions::Extension* extension);

  // Adds or removes the given |extension| from the toolbar model.
  void AddExtension(const extensions::Extension* extension);
  void RemoveExtension(const extensions::Extension* extension);

  // Looks up and returns the extension with the given |id| in the set of
  // enabled extensions.
  const extensions::Extension* GetExtensionById(const std::string& id) const;

  // Our observers.
  base::ObserverList<Observer> observers_;

  // The Profile this toolbar model is for.
  Profile* profile_;

  extensions::ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;

  // The ExtensionActionAPI object, cached for convenience.
  extensions::ExtensionActionAPI* extension_action_api_;

  // The ExtensionRegistry object, cached for convenience.
  extensions::ExtensionRegistry* extension_registry_;

  // True if we've handled the initial EXTENSIONS_READY notification.
  bool actions_initialized_;

  // If true, we include all actions in the toolbar model.
  bool use_redesign_;

  // Ordered list of browser actions.
  std::vector<ToolbarItem> toolbar_items_;

  // List of browser actions which should be highlighted.
  std::vector<ToolbarItem> highlighted_items_;

  // The current type of highlight (with HIGHLIGHT_NONE indicating no current
  // highlight).
  HighlightType highlight_type_;

  // A list of action ids ordered to correspond with their last known
  // positions.
  std::vector<std::string> last_known_positions_;

  // The number of icons visible (the rest should be hidden in the overflow
  // chevron). A value of -1 indicates that all icons should be visible.
  // Instead of using this variable directly, use visible_icon_count() if
  // possible.
  // TODO(devlin): Make a new variable to indicate that all icons should be
  // visible, instead of overloading this one.
  int visible_icon_count_;

  ScopedObserver<extensions::ExtensionActionAPI,
                 extensions::ExtensionActionAPI::Observer>
      extension_action_observer_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<extensions::ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  // For observing change of toolbar order preference by external entity (sync).
  PrefChangeRegistrar pref_change_registrar_;
  base::Closure pref_change_callback_;

  base::WeakPtrFactory<ToolbarActionsModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_
