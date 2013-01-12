// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_EVENT_ROUTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class WebContents;
}

namespace extensions {

// The BrowserEventRouter listens to Browser window & tab events
// and routes them to listeners inside extension process renderers.
// BrowserEventRouter listens to *all* events, but will only route
// events from windows/tabs within a profile to extension processes in the same
// profile.
class BrowserEventRouter : public TabStripModelObserver,
                           public chrome::BrowserListObserver,
                           public content::NotificationObserver {
 public:
  explicit BrowserEventRouter(Profile* profile);
  virtual ~BrowserEventRouter();

  // chrome::BrowserListObserver
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;
  virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

  // TabStripModelObserver
  virtual void TabInsertedAt(content::WebContents* contents, int index,
                             bool active) OVERRIDE;
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabSelectionChanged(
      TabStripModel* tab_strip_model,
      const ui::ListSelectionModel& old_model) OVERRIDE;
  virtual void TabMoved(content::WebContents* contents,
                        int from_index,
                        int to_index) OVERRIDE;
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;
  virtual void TabPinnedStateChanged(content::WebContents* contents,
                                     int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // Fires the onClicked event for page_action.
  void PageActionExecuted(Profile* profile,
                          const ExtensionAction& page_action,
                          int tab_id,
                          const std::string& url,
                          int button);

  // Fires the onClicked event for script_badge.
  void ScriptBadgeExecuted(Profile* profile,
                           const ExtensionAction& script_badge,
                           int tab_id);

  // Fires the onClicked event for browser_action.
  void BrowserActionExecuted(const ExtensionAction& browser_action,
                             Browser* browser);

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  // "Synthetic" event. Called from TabInsertedAt if new tab is detected.
  void TabCreatedAt(content::WebContents* contents, int index, bool active);

  // Internal processing of tab updated events. Is called by both TabChangedAt
  // and Observe/NAV_ENTRY_COMMITTED.
  void TabUpdated(content::WebContents* contents, bool did_navigate);

  // The DispatchEvent methods forward events to the |profile|'s event router.
  // The BrowserEventRouter listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     scoped_ptr<base::ListValue> args,
                     EventRouter::UserGestureState user_gesture);

  void DispatchEventToExtension(Profile* profile,
                                const std::string& extension_id,
                                const char* event_name,
                                scoped_ptr<base::ListValue> event_args,
                                EventRouter::UserGestureState user_gesture);

  void DispatchEventsAcrossIncognito(
      Profile* profile,
      const char* event_name,
      scoped_ptr<base::ListValue> event_args,
      scoped_ptr<base::ListValue> cross_incognito_args);

  void DispatchSimpleBrowserEvent(Profile* profile,
                                  const int window_id,
                                  const char* event_name);

  // Packages |changed_properties| as a tab updated event for the tab |contents|
  // and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               scoped_ptr<DictionaryValue> changed_properties);

  // Called to dispatch a deprecated style page action click event that was
  // registered like:
  //   chrome.pageActions["name"].addListener(function(actionId, info){})
  void DispatchOldPageActionEvent(Profile* profile,
    const std::string& extension_id,
    const std::string& page_action_id,
    int tab_id,
    const std::string& url,
    int button);

  // Register ourselves to receive the various notifications we are interested
  // in for a browser.
  void RegisterForBrowserNotifications(Browser* browser);

  // Register ourselves to receive the various notifications we are interested
  // in for a tab.
  void RegisterForTabNotifications(content::WebContents* contents);

  // Removes notifications added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(content::WebContents* contents);

  content::NotificationRegistrar registrar_;

  // Maintain some information about known tabs, so we can:
  //
  //  - distinguish between tab creation and tab insertion
  //  - not send tab-detached after tab-removed
  //  - reduce the "noise" of TabChangedAt() when sending events to extensions
  class TabEntry {
   public:
    // Create a new tab entry whose initial state is TAB_COMPLETE.  This
    // constructor is required because TabEntry objects placed inside an
    // std::map<> by value.
    TabEntry();

    // Update the load state of the tab based on its WebContents.  Returns true
    // if the state changed, false otherwise.  Whether the state has changed or
    // not is used to determine if events needs to be sent to extensions during
    // processing of TabChangedAt(). This method will "hold" a state-change
    // to "loading", until the DidNavigate() method which should always follow
    // it. Returns NULL if no updates should be sent.
    DictionaryValue* UpdateLoadState(const content::WebContents* contents);

    // Indicates that a tab load has resulted in a navigation and the
    // destination url is available for inspection. Returns NULL if no updates
    // should be sent.
    DictionaryValue* DidNavigate(const content::WebContents* contents);

   private:
    // Whether we are waiting to fire the 'complete' status change. This will
    // occur the first time the WebContents stops loading after the
    // NAV_ENTRY_COMMITTED was fired. The tab may go back into and out of the
    // loading state subsequently, but we will ignore those changes.
    bool complete_waiting_on_load_;

    GURL url_;
  };

  // Gets the TabEntry for the given |contents|. Returns TabEntry* if
  // found, NULL if not.
  TabEntry* GetTabEntry(const content::WebContents* contents);

  // Called when either a browser or page action is executed. Figures out which
  // event to send based on what the extension wants.
  void ExtensionActionExecuted(Profile* profile,
                               const ExtensionAction& extension_action,
                               content::WebContents* web_contents);

  std::map<int, TabEntry> tab_entries_;

  // The main profile that owns this event router.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_EVENT_ROUTER_H_
