// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/common/notification_registrar.h"
#if defined(TOOLKIT_VIEWS)
#include "views/focus/focus_manager.h"
#include "views/view.h"
#elif defined(TOOLKIT_GTK)
#include "ui/base/x/active_window_watcher_x.h"
#endif

// The ExtensionBrowserEventRouter listens to Browser window & tab events
// and routes them to listeners inside extension process renderers.
// ExtensionBrowserEventRouter listens to *all* events, but will only route
// events from windows/tabs within a profile to extension processes in the same
// profile.
class ExtensionBrowserEventRouter : public TabStripModelObserver,
#if defined(TOOLKIT_VIEWS)
                                    public views::WidgetFocusChangeListener,
#elif defined(TOOLKIT_GTK)
                                    public ui::ActiveWindowWatcherX::Observer,
#endif
                                    public BrowserList::Observer,
                                    public NotificationObserver {
 public:
  explicit ExtensionBrowserEventRouter(Profile* profile);
  ~ExtensionBrowserEventRouter();

  // Must be called once. Subsequent calls have no effect.
  void Init();

  // BrowserList::Observer
  virtual void OnBrowserAdded(const Browser* browser);
  virtual void OnBrowserRemoved(const Browser* browser);
  virtual void OnBrowserSetLastActive(const Browser* browser);

#if defined(TOOLKIT_VIEWS)
  virtual void NativeFocusWillChange(gfx::NativeView focused_before,
                                     gfx::NativeView focused_now);
#elif defined(TOOLKIT_GTK)
  virtual void ActiveWindowChanged(GdkWindow* active_window);
#endif

  // Called from Observe() on BROWSER_WINDOW_READY (not a part of
  // BrowserList::Observer).
  void OnBrowserWindowReady(const Browser* browser);

  // TabStripModelObserver
  virtual void TabInsertedAt(TabContentsWrapper* contents, int index,
                             bool foreground);
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index);
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContentsWrapper* contents, int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContentsWrapper* contents, int index,
                            TabChangeType change_type);
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index);
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents, int index);
  virtual void TabStripEmpty();

  // Page Action execute event.
  void PageActionExecuted(Profile* profile,
                          const std::string& extension_id,
                          const std::string& page_action_id,
                          int tab_id,
                          const std::string& url,
                          int button);
  // Browser Actions execute event.
  void BrowserActionExecuted(Profile* profile,
                             const std::string& extension_id,
                             Browser* browser);

  // NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  // "Synthetic" event. Called from TabInsertedAt if new tab is detected.
  void TabCreatedAt(TabContents* contents, int index, bool foreground);

  // Internal processing of tab updated events. Is called by both TabChangedAt
  // and Observe/NAV_ENTRY_COMMITTED.
  void TabUpdated(TabContents* contents, bool did_navigate);

  // Packages |changed_properties| as a tab updated event for the tab |contents|
  // and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(TabContents* contents,
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
  void RegisterForTabNotifications(TabContents* contents);

  // Removes notifications added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(TabContents* contents);

  NotificationRegistrar registrar_;

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

    // Update the load state of the tab based on its TabContents.  Returns true
    // if the state changed, false otherwise.  Whether the state has changed or
    // not is used to determine if events needs to be sent to extensions during
    // processing of TabChangedAt(). This method will "hold" a state-change
    // to "loading", until the DidNavigate() method which should always follow
    // it. Returns NULL if no updates should be sent.
    DictionaryValue* UpdateLoadState(const TabContents* contents);

    // Indicates that a tab load has resulted in a navigation and the
    // destination url is available for inspection. Returns NULL if no updates
    // should be sent.
    DictionaryValue* DidNavigate(const TabContents* contents);

   private:
    // Whether we are waiting to fire the 'complete' status change. This will
    // occur the first time the TabContents stops loading after the
    // NAV_ENTRY_COMMITTED was fired. The tab may go back into and out of the
    // loading state subsequently, but we will ignore those changes.
    bool complete_waiting_on_load_;

    GURL url_;
  };

  // Gets the TabEntry for the given |contents|. Returns TabEntry* if
  // found, NULL if not.
  TabEntry* GetTabEntry(const TabContents* contents);

  std::map<int, TabEntry> tab_entries_;

  // The currently focused window. We keep this so as to avoid sending multiple
  // windows.onFocusChanged events with the same windowId.
  int focused_window_id_;

  // The main profile (non-OTR) profile which will be used to send events not
  // associated with any browser.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBrowserEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
