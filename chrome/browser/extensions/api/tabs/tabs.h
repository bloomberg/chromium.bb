// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_H__
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_H__

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class BackingStore;
class GURL;
class PrefServiceSyncable;
class SkBitmap;

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace skia {
class PlatformBitmap;
}

// Windows
class GetWindowFunction : public SyncExtensionFunction {
  virtual ~GetWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.get")
};
class GetCurrentWindowFunction : public SyncExtensionFunction {
  virtual ~GetCurrentWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getCurrent")
};
class GetLastFocusedWindowFunction : public SyncExtensionFunction {
  virtual ~GetLastFocusedWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getLastFocused")
};
class GetAllWindowsFunction : public SyncExtensionFunction {
  virtual ~GetAllWindowsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getAll")
};
class CreateWindowFunction : public SyncExtensionFunction {
  virtual ~CreateWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  // Returns whether the window should be created in incognito mode.
  // |urls| is the list of urls to open. If we are creating an incognito window,
  // the function will remove these urls which may not be opened in incognito
  // mode.  If window creation leads the browser into an erroneous state,
  // |is_error| is set to true (also, error_ member variable is assigned
  // the proper error message).
  bool ShouldOpenIncognitoWindow(const base::DictionaryValue* args,
                                 std::vector<GURL>* urls,
                                 bool* is_error);
  DECLARE_EXTENSION_FUNCTION_NAME("windows.create")
};
class UpdateWindowFunction : public SyncExtensionFunction {
  virtual ~UpdateWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.update")
};
class RemoveWindowFunction : public SyncExtensionFunction {
  virtual ~RemoveWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.remove")
};

// Tabs
class GetTabFunction : public SyncExtensionFunction {
  virtual ~GetTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.get")
};
class GetCurrentTabFunction : public SyncExtensionFunction {
  virtual ~GetCurrentTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getCurrent")
};
class GetSelectedTabFunction : public SyncExtensionFunction {
  virtual ~GetSelectedTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getSelected")
};
class GetAllTabsInWindowFunction : public SyncExtensionFunction {
  virtual ~GetAllTabsInWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getAllInWindow")
};
class QueryTabsFunction : public SyncExtensionFunction {
  virtual ~QueryTabsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.query")
};
class CreateTabFunction : public SyncExtensionFunction {
  virtual ~CreateTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.create")
};
class DuplicateTabFunction : public SyncExtensionFunction {
  virtual ~DuplicateTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.duplicate")
};
class HighlightTabsFunction : public SyncExtensionFunction {
  virtual ~HighlightTabsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.highlight")
};
class UpdateTabFunction : public AsyncExtensionFunction {
 public:
  UpdateTabFunction();

 protected:
  virtual ~UpdateTabFunction() {}
  virtual bool UpdateURLIfPresent(base::DictionaryValue* update_props,
                                  int tab_id,
                                  bool* is_async);
  virtual void PopulateResult();

  content::WebContents* web_contents_;

 private:
  virtual bool RunImpl() OVERRIDE;
  void OnExecuteCodeFinished(const std::string& error,
                             int32 on_page_id,
                             const GURL& on_url,
                             const ListValue& script_result);

  DECLARE_EXTENSION_FUNCTION_NAME("tabs.update")
};
class MoveTabsFunction : public SyncExtensionFunction {
  virtual ~MoveTabsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.move")
};
class ReloadTabFunction : public SyncExtensionFunction {
  virtual ~ReloadTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.reload")
};
class RemoveTabsFunction : public SyncExtensionFunction {
  virtual ~RemoveTabsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.remove")
};
class DetectTabLanguageFunction : public AsyncExtensionFunction,
                                  public content::NotificationObserver {
 private:
  virtual ~DetectTabLanguageFunction() {}
  virtual bool RunImpl() OVERRIDE;

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  void GotLanguage(const std::string& language);
  content::NotificationRegistrar registrar_;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.detectLanguage")
};
class CaptureVisibleTabFunction : public AsyncExtensionFunction,
                                  public content::NotificationObserver {
 public:
  static void RegisterUserPrefs(PrefServiceSyncable* service);

 protected:
  enum ImageFormat {
    FORMAT_JPEG,
    FORMAT_PNG
  };

  // The default quality setting used when encoding jpegs.
  static const int kDefaultQuality;

  virtual ~CaptureVisibleTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  virtual bool GetTabToCapture(content::WebContents** web_contents);
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  void SendResultFromBitmap(const SkBitmap& screen_capture);

 private:
  void CopyFromBackingStoreComplete(skia::PlatformBitmap* bitmap,
                                    bool succeeded);

  content::NotificationRegistrar registrar_;

  // The format (JPEG vs PNG) of the resulting image.  Set in RunImpl().
  ImageFormat image_format_;

  // Quality setting to use when encoding jpegs.  Set in RunImpl().
  int image_quality_;

  DECLARE_EXTENSION_FUNCTION_NAME("tabs.captureVisibleTab")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_H__
