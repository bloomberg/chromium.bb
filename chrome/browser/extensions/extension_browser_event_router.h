// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_registrar.h"
#if defined(TOOLKIT_VIEWS)
#include "ui/views/focus/widget_focus_manager.h"
#elif defined(TOOLKIT_GTK)
#include "ui/base/x/active_window_watcher_x_observer.h"
#endif

namespace content {
class WebContents;
}

// The ExtensionBrowserEventRouter listens to Browser window & tab events
// and routes them to listeners inside extension process renderers.
// ExtensionBrowserEventRouter listens to *all* events, but will only route
// events from windows/tabs within a profile to extension processes in the same
// profile.
class ExtensionBrowserEventRouter : public TabStripModelObserver,
#if defined(TOOLKIT_VIEWS)
                                    public views::WidgetFocusChangeListener,
#elif defined(TOOLKIT_GTK)
                                    public ui::ActiveWindowWatcherXObserver,
#endif
                                    public BrowserList::Observer,
                                    public ExtensionToolbarModel::Observer,
                                    public content::NotificationObserver {
 public:
  explicit ExtensionBrowserEventRouter(Profile* profile);
  virtual ~ExtensionBrowserEventRouter();

  // Must be called once. Subsequent calls have no effect.
  void Init(ExtensionToolbarModel* model);

  // BrowserList::Observer
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;
  virtual void OnBrowserSetLastActive(const Browser* browser) OVERRIDE;

#if defined(TOOLKIT_VIEWS)
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;
#elif defined(TOOLKIT_GTK)
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;
#endif

  // Called from Observe() on BROWSER_WINDOW_READY (not a part of
  // BrowserList::Observer).
  void OnBrowserWindowReady(const Browser* browser);

  // TabStripModelObserver
  virtual void TabInsertedAt(TabContentsWrapper* contents, int index,
                             bool active) OVERRIDE;
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) OVERRIDE;
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) OVERRIDE;
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabSelectionChanged(
      TabStripModel* tab_strip_model,
      const TabStripSelectionModel& old_model) OVERRIDE;
  virtual void TabMoved(TabContentsWrapper* contents, int from_index,
                        int to_index) OVERRIDE;
  virtual void TabChangedAt(TabContentsWrapper* contents, int index,
                            TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) OVERRIDE;
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents,
                                     int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // Page Action execute event.
  void PageActionExecuted(Profile* profile,
                          const std::string& extension_id,
                          const std::string& page_action_id,
                          int tab_id,
                          const std::string& url,
                          int button);

  // A keyboard shortcut resulted in an extension command.
  void CommandExecuted(Profile* profile,
                       const std::string& extension_id,
                       const std::string& command);

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

  // ExtensionToolbarModel::Observer. Browser Actions execute event.
  virtual void BrowserActionExecuted(const std::string& extension_id,
                                     Browser* browser) OVERRIDE;

  // The DispatchEvent methods forward events to the |profile|'s event router.
  // The ExtensionBrowserEventRouter listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  void DispatchEventToExtension(Profile* profile,
                                const std::string& extension_id,
                                const char* event_name,
                                const std::string& json_args);

  void DispatchEventsAcrossIncognito(Profile* profile,
                                     const char* event_name,
                                     const std::string& json_args,
                                     const std::string& cross_incognito_args);

  void DispatchEventWithTab(Profile* profile,
                            const std::string& extension_id,
                            const char* event_name,
                            const content::WebContents* web_contents,
                            bool active);

  void DispatchSimpleBrowserEvent(Profile* profile,
                                  const int window_id,
                                  const char* event_name);

  // Packages |changed_properties| as a tab updated event for the tab |contents|
  // and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               DictionaryValue* changed_properties);

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
  void RegisterForBrowserNotifications(const Browser* browser);

  // Register ourselves to receive the various notifications we are interested
  // in for a tab.
  void RegisterForTabNotifications(content::WebContents* contents);

  // Removes notifications added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(content::WebContents* contents);

  content::NotificationRegistrar registrar_;

  bool initialized_;

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
                               const std::string& extension_id,
                               TabContentsWrapper* tab_contents);

  std::map<int, TabEntry> tab_entries_;

  // The main profile that owns this event router.
  Profile* profile_;

  // The profile the currently focused window belongs to; either the main or
  // incognito profile or NULL (none of the above). We remember this in order
  // to correctly handle focus changes between non-OTR and OTR windows.
  Profile* focused_profile_;

  // The currently focused window. We keep this so as to avoid sending multiple
  // windows.onFocusChanged events with the same windowId.
  int focused_window_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBrowserEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
