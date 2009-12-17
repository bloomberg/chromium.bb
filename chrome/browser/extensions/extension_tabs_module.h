// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class DictionaryValue;
class ListValue;
class TabContents;
class TabStripModel;

class ExtensionTabUtil {
 public:
  // Possible tab states.  These states are used to calculate the "status"
  // property of the Tab object that is used in the extension tab API.
  enum TabStatus {
    TAB_LOADING,  // Waiting for the DOM to load.
    TAB_COMPLETE  // Tab loading and rendering is complete.
  };

  static int GetWindowId(const Browser* browser);
  static int GetTabId(const TabContents* tab_contents);
  static TabStatus GetTabStatus(const TabContents* tab_contents);
  static std::string GetTabStatusText(TabStatus status);
  static int GetWindowIdOfTab(const TabContents* tab_contents);
  static ListValue* CreateTabList(const Browser* browser);
  static DictionaryValue* CreateTabValue(const TabContents* tab_contents);
  static DictionaryValue* CreateTabValue(const TabContents* tab_contents,
                                         TabStripModel* tab_strip,
                                         int tab_index);
  static DictionaryValue* CreateWindowValue(const Browser* browser,
                                            bool populate_tabs);

  static bool GetDefaultTab(Browser* browser, TabContents** contents,
                            int* tab_id);
  // Any out parameter (|browser|, |tab_strip|, |contents|, & |tab_index|) may
  // be NULL and will not be set within the function.
  static bool GetTabById(int tab_id, Profile* profile, Browser** browser,
                         TabStripModel** tab_strip,
                         TabContents** contents,
                         int* tab_index);
};

// Windows
class GetWindowFunction : public SyncExtensionFunction {
  ~GetWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.get")
};
class GetCurrentWindowFunction : public SyncExtensionFunction {
  ~GetCurrentWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getCurrent")
};
class GetLastFocusedWindowFunction : public SyncExtensionFunction {
  ~GetLastFocusedWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getLastFocused")
};
class GetAllWindowsFunction : public SyncExtensionFunction {
  ~GetAllWindowsFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getAll")
};
class CreateWindowFunction : public SyncExtensionFunction {
  ~CreateWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.create")
};
class UpdateWindowFunction : public SyncExtensionFunction {
  ~UpdateWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.update")
};
class RemoveWindowFunction : public SyncExtensionFunction {
  ~RemoveWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("windows.remove")
};

// Tabs
class GetTabFunction : public SyncExtensionFunction {
  ~GetTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.get")
};
class GetSelectedTabFunction : public SyncExtensionFunction {
  ~GetSelectedTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getSelected")
};
class GetAllTabsInWindowFunction : public SyncExtensionFunction {
  ~GetAllTabsInWindowFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getAllInWindow")
};
class CreateTabFunction : public SyncExtensionFunction {
  ~CreateTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.create")
};
class UpdateTabFunction : public SyncExtensionFunction {
  ~UpdateTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.update")
};
class MoveTabFunction : public SyncExtensionFunction {
  ~MoveTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.move")
};
class RemoveTabFunction : public SyncExtensionFunction {
  ~RemoveTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.remove")
};
class DetectTabLanguageFunction : public AsyncExtensionFunction,
                                  public NotificationObserver {
 private:
  ~DetectTabLanguageFunction() {}
  virtual bool RunImpl();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  NotificationRegistrar registrar_;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.detectLanguage")
};
class CaptureVisibleTabFunction : public SyncExtensionFunction {
  ~CaptureVisibleTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.captureVisibleTab")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
