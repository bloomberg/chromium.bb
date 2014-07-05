// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/capture_web_contents_function.h"
#include "chrome/browser/extensions/api/execute_code_function.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/extensions/api/tabs.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

class GURL;
class SkBitmap;
class TabStripModel;

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

// Converts a ZoomMode to its ZoomSettings representation.
void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings);

// Windows
class WindowsGetFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsGetFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsGetCurrentFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsGetLastFocusedFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsGetAllFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsCreateFunction() {}
  virtual bool RunSync() OVERRIDE;
  // Returns whether the window should be created in incognito mode.
  // |create_data| are the options passed by the extension. It may be NULL.
  // |urls| is the list of urls to open. If we are creating an incognito window,
  // the function will remove these urls which may not be opened in incognito
  // mode.  If window creation leads the browser into an erroneous state,
  // |is_error| is set to true (also, error_ member variable is assigned
  // the proper error message).
  bool ShouldOpenIncognitoWindow(
      const api::windows::Create::Params::CreateData* create_data,
      std::vector<GURL>* urls,
      bool* is_error);
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)
};
class WindowsUpdateFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsUpdateFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)
};
class WindowsRemoveFunction : public ChromeSyncExtensionFunction {
  virtual ~WindowsRemoveFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

// Tabs
class TabsGetFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsGetFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};
class TabsGetCurrentFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsGetCurrentFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};
class TabsGetSelectedFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsGetSelectedFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.getSelected", TABS_GETSELECTED)
};
class TabsGetAllInWindowFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsGetAllInWindowFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.getAllInWindow", TABS_GETALLINWINDOW)
};
class TabsQueryFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsQueryFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
};
class TabsCreateFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsCreateFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};
class TabsDuplicateFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsDuplicateFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
};
class TabsHighlightFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsHighlightFunction() {}
  virtual bool RunSync() OVERRIDE;
  bool HighlightTab(TabStripModel* tabstrip,
                    ui::ListSelectionModel* selection,
                    int *active_index,
                    int index);
  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
};
class TabsUpdateFunction : public ChromeAsyncExtensionFunction {
 public:
  TabsUpdateFunction();

 protected:
  virtual ~TabsUpdateFunction() {}
  virtual bool UpdateURL(const std::string& url,
                         int tab_id,
                         bool* is_async);
  virtual void PopulateResult();

  content::WebContents* web_contents_;

 private:
  virtual bool RunAsync() OVERRIDE;
  void OnExecuteCodeFinished(const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& script_result);

  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
};
class TabsMoveFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsMoveFunction() {}
  virtual bool RunSync() OVERRIDE;
  bool MoveTab(int tab_id,
               int* new_index,
               int iteration,
               base::ListValue* tab_values,
               int* window_id);
  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};
class TabsReloadFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsReloadFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};
class TabsRemoveFunction : public ChromeSyncExtensionFunction {
  virtual ~TabsRemoveFunction() {}
  virtual bool RunSync() OVERRIDE;
  bool RemoveTab(int tab_id);
  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
};
class TabsDetectLanguageFunction : public ChromeAsyncExtensionFunction,
                                   public content::NotificationObserver {
 private:
  virtual ~TabsDetectLanguageFunction() {}
  virtual bool RunAsync() OVERRIDE;

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  void GotLanguage(const std::string& language);
  content::NotificationRegistrar registrar_;
  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
};
class TabsCaptureVisibleTabFunction
    : public extensions::CaptureWebContentsFunction {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  virtual ~TabsCaptureVisibleTabFunction() {}

 private:
  // extensions::CaptureWebContentsFunction:
  virtual bool IsScreenshotEnabled() OVERRIDE;
  virtual content::WebContents* GetWebContentsForID(int id) OVERRIDE;
  virtual void OnCaptureFailure(FailureReason reason) OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)
};

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public ExecuteCodeFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  virtual ~ExecuteCodeInTabFunction();

  // ExtensionFunction:
  virtual bool HasPermission() OVERRIDE;

  // Initialize the |execute_tab_id_| and |details_| if they haven't already
  // been. Returns whether initialization was successful.
  virtual bool Init() OVERRIDE;
  virtual bool CanExecuteScriptOnPage() OVERRIDE;
  virtual ScriptExecutor* GetScriptExecutor() OVERRIDE;
  virtual bool IsWebView() const OVERRIDE;
  virtual const GURL& GetWebViewSrc() const OVERRIDE;

 private:
  // Id of tab which executes code.
  int execute_tab_id_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 protected:
  virtual bool ShouldInsertCSS() const OVERRIDE;

 private:
  virtual ~TabsExecuteScriptFunction() {}

  virtual void OnExecuteCodeFinished(
      const std::string& error,
      const GURL& on_url,
      const base::ListValue& script_result) OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  virtual ~TabsInsertCSSFunction() {}

  virtual bool ShouldInsertCSS() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.insertCSS", TABS_INSERTCSS)
};

class ZoomAPIFunction : public ChromeAsyncExtensionFunction {
 protected:
  virtual ~ZoomAPIFunction() {}

  // Gets the WebContents for |tab_id| if it is specified. Otherwise get the
  // WebContents for the active tab in the current window. Calling this function
  // may set error_.
  //
  // TODO(...) many other tabs API functions use similar behavior. There should
  // be a way to share this implementation somehow.
  content::WebContents* GetWebContents(int tab_id);
};

class TabsSetZoomFunction : public ZoomAPIFunction {
 private:
  virtual ~TabsSetZoomFunction() {}

  virtual bool RunAsync() OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public ZoomAPIFunction {
 private:
  virtual ~TabsGetZoomFunction() {}

  virtual bool RunAsync() OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public ZoomAPIFunction {
 private:
  virtual ~TabsSetZoomSettingsFunction() {}

  virtual bool RunAsync() OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public ZoomAPIFunction {
 private:
  virtual ~TabsGetZoomSettingsFunction() {}

  virtual bool RunAsync() OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
