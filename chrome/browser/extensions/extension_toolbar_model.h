// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class ExtensionService;
class PrefService;

// Model for the browser actions toolbar.
class ExtensionToolbarModel : public content::NotificationObserver,
                              public BrowserContextKeyedService   {
 public:
  ExtensionToolbarModel(Profile* profile,
                        extensions::ExtensionPrefs* extension_prefs);
  virtual ~ExtensionToolbarModel();

  // The action that should be taken as a result of clicking a browser action.
  enum Action {
    ACTION_NONE,
    ACTION_SHOW_POPUP,
    // Unlike LocationBarController there is no ACTION_SHOW_CONTEXT_MENU,
    // because UI implementations tend to handle this themselves at a higher
    // level.
  };

  // A class which is informed of changes to the model; represents the view of
  // MVC. Also used for signaling view changes such as showing extension popups.
  class Observer {
   public:
    // An extension with a browser action button has been added, and should go
    // in the toolbar at |index|.
    virtual void BrowserActionAdded(const extensions::Extension* extension,
                                    int index) {}

    // The browser action button for |extension| should no longer show.
    virtual void BrowserActionRemoved(const extensions::Extension* extension) {}

    // The browser action button for |extension| has been moved to |index|.
    virtual void BrowserActionMoved(const extensions::Extension* extension,
                                    int index) {}

    // Signal the |extension| to show the popup now in the active window.
    // Returns true if a popup was slated to be shown.
    virtual bool BrowserActionShowPopup(const extensions::Extension* extension);

    // Called when the model has finished loading.
    virtual void ModelLoaded() {}

   protected:
    virtual ~Observer() {}
  };

  // Convenience function to get the ExtensionToolbarModel for a Profile.
  static ExtensionToolbarModel* Get(Profile* profile);

  // Functions called by the view.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void MoveBrowserAction(const extensions::Extension* extension, int index);
  // Executes the browser action for an extension and returns the action that
  // the UI should perform in response.
  // |popup_url_out| will be set if the extension should show a popup, with
  // the URL that should be shown, if non-NULL. |should_grant| controls whether
  // the extension should be granted page tab permissions, which is what happens
  // when the user clicks the browser action, but not, for example, when the
  // showPopup API is called.
  Action ExecuteBrowserAction(const extensions::Extension* extension,
                              Browser* browser,
                              GURL* popup_url_out,
                              bool should_grant);
  // If count == size(), this will set the visible icon count to -1, meaning
  // "show all actions".
  void SetVisibleIconCount(int count);
  // As above, a return value of -1 represents "show all actions".
  int GetVisibleIconCount() const { return visible_icon_count_; }

  bool extensions_initialized() const { return extensions_initialized_; }

  const extensions::ExtensionList& toolbar_items() const {
    return toolbar_items_;
  }

  // Utility functions for converting between an index into the list of
  // incognito-enabled browser actions, and the list of all browser actions.
  int IncognitoIndexToOriginal(int incognito_index);
  int OriginalIndexToIncognito(int original_index);

  void OnExtensionToolbarPrefChange();

  // Tells observers to display a popup without granting tab permissions and
  // returns whether the popup was slated to be shown.
  bool ShowBrowserActionPopup(const extensions::Extension* extension);

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // To be called after the extension service is ready; gets loaded extensions
  // from the extension service and their saved order from the pref service
  // and constructs |toolbar_items_| from these data.
  void InitializeExtensionList(ExtensionService* service);
  void Populate(const extensions::ExtensionIdList& positions,
                ExtensionService* service);

  // Fills |list| with extensions based on provided |order|.
  void FillExtensionList(const extensions::ExtensionIdList& order,
                         ExtensionService* service);

  // Save the model to prefs.
  void UpdatePrefs();

  // Finds the last known visible position of the icon for an |extension|. The
  // value returned is a zero-based index into the vector of visible items.
  size_t FindNewPositionFromLastKnownGood(
      const extensions::Extension* extension);

  // Our observers.
  ObserverList<Observer> observers_;

  void AddExtension(const extensions::Extension* extension);
  void RemoveExtension(const extensions::Extension* extension);
  void UninstalledExtension(const extensions::Extension* extension);

  // The Profile this toolbar model is for.
  Profile* profile_;

  extensions::ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;

  // True if we've handled the initial EXTENSIONS_READY notification.
  bool extensions_initialized_;

  // Ordered list of browser action buttons.
  extensions::ExtensionList toolbar_items_;

  extensions::ExtensionIdList last_known_positions_;

  // The number of icons visible (the rest should be hidden in the overflow
  // chevron).
  int visible_icon_count_;

  content::NotificationRegistrar registrar_;

  // For observing change of toolbar order preference by external entity (sync).
  PrefChangeRegistrar pref_change_registrar_;
  base::Closure pref_change_callback_;

  base::WeakPtrFactory<ExtensionToolbarModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionToolbarModel);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
