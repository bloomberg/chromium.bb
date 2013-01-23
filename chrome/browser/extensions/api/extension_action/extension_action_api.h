// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace extensions {
class TabHelper;

class ExtensionActionAPI : public ProfileKeyedAPI {
 public:
  explicit ExtensionActionAPI(Profile* profile);
  virtual ~ExtensionActionAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ExtensionActionAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<ExtensionActionAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() { return "ExtensionActionAPI"; }

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionAPI);
};

// This class manages reading and writing browser action values from storage.
class ExtensionActionStorageManager
    : public content::NotificationObserver,
      public base::SupportsWeakPtr<ExtensionActionStorageManager> {
 public:
  explicit ExtensionActionStorageManager(Profile* profile);
  virtual ~ExtensionActionStorageManager();

 private:
  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Reads/Writes the ExtensionAction's default values to/from storage.
  void WriteToStorage(ExtensionAction* extension_action);
  void ReadFromStorage(
      const std::string& extension_id, scoped_ptr<base::Value> value);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
};

// Implementation of the browserAction, pageAction, and scriptBadge APIs.
//
// Divergent behaviour between the three is minimal (pageAction and scriptBadge
// have required tabIds while browserAction's are optional, they have different
// internal browser notification requirements, and not all functions are defined
// for all APIs).
class ExtensionActionFunction : public SyncExtensionFunction {
 public:
  static bool ParseCSSColorString(const std::string& color_string,
                                  SkColor* result);

 protected:
  ExtensionActionFunction();
  virtual ~ExtensionActionFunction();
  virtual bool RunImpl() OVERRIDE;
  virtual bool RunExtensionAction() = 0;

  bool ExtractDataFromArguments();
  void NotifyChange();
  void NotifyBrowserActionChange();
  void NotifyLocationBarChange();
  void NotifySystemIndicatorChange();
  bool SetVisible(bool visible);

  // Extension-related information for |tab_id_|.
  // CHECK-fails if there is no tab.
  extensions::TabHelper& tab_helper() const;

  // All the extension action APIs take a single argument called details that
  // is a dictionary.
  base::DictionaryValue* details_;

  // The tab id the extension action function should apply to, if any, or
  // kDefaultTabId if none was specified.
  int tab_id_;

  // WebContents for |tab_id_| if one exists.
  content::WebContents* contents_;

  // The extension action for the current extension.
  ExtensionAction* extension_action_;
};

//
// Implementations of each extension action API.
//
// pageAction and browserAction bindings are created for these by extending them
// then declaring an EXTENSION_FUNCTION_NAME.
//

// show
class ExtensionActionShowFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionShowFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// hide
class ExtensionActionHideFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionHideFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// setIcon
class ExtensionActionSetIconFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionSetIconFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// setTitle
class ExtensionActionSetTitleFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionSetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// setPopup
class ExtensionActionSetPopupFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionSetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// setBadgeText
class ExtensionActionSetBadgeTextFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionSetBadgeTextFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// setBadgeBackgroundColor
class ExtensionActionSetBadgeBackgroundColorFunction
    : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionSetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// getTitle
class ExtensionActionGetTitleFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionGetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// getPopup
class ExtensionActionGetPopupFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionGetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// getBadgeText
class ExtensionActionGetBadgeTextFunction : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionGetBadgeTextFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

// getBadgeBackgroundColor
class ExtensionActionGetBadgeBackgroundColorFunction
    : public ExtensionActionFunction {
 protected:
  virtual ~ExtensionActionGetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
};

//
// browserAction.* aliases for supported browserAction APIs.
//

class BrowserActionSetIconFunction : public ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setIcon", BROWSERACTION_SETICON)

 protected:
  virtual ~BrowserActionSetIconFunction() {}
};

class BrowserActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setTitle", BROWSERACTION_SETTITLE)

 protected:
  virtual ~BrowserActionSetTitleFunction() {}
};

class BrowserActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setPopup", BROWSERACTION_SETPOPUP)

 protected:
  virtual ~BrowserActionSetPopupFunction() {}
};

class BrowserActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getTitle", BROWSERACTION_GETTITLE)

 protected:
  virtual ~BrowserActionGetTitleFunction() {}
};

class BrowserActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getPopup", BROWSERACTION_GETPOPUP)

 protected:
  virtual ~BrowserActionGetPopupFunction() {}
};

class BrowserActionSetBadgeTextFunction
    : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeText",
                             BROWSERACTION_SETBADGETEXT)

 protected:
  virtual ~BrowserActionSetBadgeTextFunction() {}
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeBackgroundColor",
                             BROWSERACTION_SETBADGEBACKGROUNDCOLOR)

 protected:
  virtual ~BrowserActionSetBadgeBackgroundColorFunction() {}
};

class BrowserActionGetBadgeTextFunction
    : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeText",
                             BROWSERACTION_GETBADGETEXT)

 protected:
  virtual ~BrowserActionGetBadgeTextFunction() {}
};

class BrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeBackgroundColor",
                             BROWSERACTION_GETBADGEBACKGROUNDCOLOR)

 protected:
  virtual ~BrowserActionGetBadgeBackgroundColorFunction() {}
};

class BrowserActionEnableFunction : public ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.enable", BROWSERACTION_ENABLE)

 protected:
  virtual ~BrowserActionEnableFunction() {}
};

class BrowserActionDisableFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.disable", BROWSERACTION_DISABLE)

 protected:
  virtual ~BrowserActionDisableFunction() {}
};

//
// scriptBadge.* aliases for supported scriptBadge APIs.
//

class ScriptBadgeSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scriptBadge.setPopup", SCRIPTBADGE_SETPOPUP)

 protected:
  virtual ~ScriptBadgeSetPopupFunction() {}
};

class ScriptBadgeGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scriptBadge.getPopup", SCRIPTBADGE_GETPOPUP)

 protected:
  virtual ~ScriptBadgeGetPopupFunction() {}
};

// scriptBadge.getAttention(tabId)
class ScriptBadgeGetAttentionFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scriptBadge.getAttention",
                             SCRIPTBADGE_GETATTENTION)

  virtual bool RunExtensionAction() OVERRIDE;

 protected:
  virtual ~ScriptBadgeGetAttentionFunction();
};

}  // namespace extensions

//
// pageAction.* aliases for supported pageAction APIs.
//

class PageActionShowFunction : public extensions::ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.show", PAGEACTION_SHOW)

 protected:
  virtual ~PageActionShowFunction() {}
};

class PageActionHideFunction : public extensions::ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.hide", PAGEACTION_HIDE)

 protected:
  virtual ~PageActionHideFunction() {}
};

class PageActionSetIconFunction
    : public extensions::ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setIcon", PAGEACTION_SETICON)

 protected:
  virtual ~PageActionSetIconFunction() {}
};

class PageActionSetTitleFunction
    : public extensions::ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setTitle", PAGEACTION_SETTITLE)

 protected:
  virtual ~PageActionSetTitleFunction() {}
};

class PageActionSetPopupFunction
    : public extensions::ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setPopup", PAGEACTION_SETPOPUP)

 protected:
  virtual ~PageActionSetPopupFunction() {}
};

class PageActionGetTitleFunction
    : public extensions::ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getTitle", PAGEACTION_GETTITLE)

 protected:
  virtual ~PageActionGetTitleFunction() {}
};

class PageActionGetPopupFunction
    : public extensions::ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getPopup", PAGEACTION_GETPOPUP)

 protected:
  virtual ~PageActionGetPopupFunction() {}
};

// Base class for deprecated page actions APIs
class PageActionsFunction : public SyncExtensionFunction {
 protected:
  PageActionsFunction();
  virtual ~PageActionsFunction();
  bool SetPageActionEnabled(bool enable);
};

// Implement chrome.pageActions.enableForTab().
class EnablePageActionsFunction : public PageActionsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.enableForTab",
                             PAGEACTIONS_ENABLEFORTAB)

 protected:
  virtual ~EnablePageActionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implement chrome.pageActions.disableForTab().
class DisablePageActionsFunction : public PageActionsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.disableForTab",
                             PAGEACTIONS_DISABLEFORTAB)

 protected:
  virtual ~DisablePageActionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
