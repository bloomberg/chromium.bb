// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the classes to realize the Font Settings Extension API as specified
// in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#pragma once

#include <map>
#include <string>
#include <utility>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_change_registrar.h"

// This class observes pref changed events on a profile and dispatches the
// corresponding extension API events to extensions.
class ExtensionFontSettingsEventRouter : public content::NotificationObserver {
 public:
  // Constructor for observing pref changed events on |profile|. Stores a
  // pointer to |profile| but does not take ownership. |profile| must be
  // non-NULL and remain alive for the lifetime of the instance. Init() must be
  // called to start observing pref changed events.
  explicit ExtensionFontSettingsEventRouter(Profile* profile);
  virtual ~ExtensionFontSettingsEventRouter();

  // Starts observing pref changed events on the profile. Must not be called
  // more than once.
  void Init();

 private:
  typedef std::pair<std::string, std::string> EventAndKeyPair;
  // Map of pref name to a pair of event name and key (defined in the extension
  // API) for dispatching a pref changed event. For example,
  // "webkit.webprefs.default_font_size" to ("onDefaultFontSizedChanged",
  // "pixelSize").
  typedef std::map<std::string, EventAndKeyPair> PrefEventMap;

  // Observes browser pref |pref_name|. When a change is observed, dispatches
  // event |event_name| to extensions. A JavaScript object is passed to the
  // extension event function with the new value of the pref in property |key|.
  void AddPrefToObserve(const char* pref_name,
                        const char* event_name,
                        const char* key);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Dispatches a changed event for the font setting for |generic_family| and
  // |script| to extensions. The new value of the setting is the value of
  // browser pref |pref_name|. If the pref changed on the incognito profile,
  // |incognito| must be set to true for extensions to get the appropriate
  // event.
  void OnFontNamePrefChanged(PrefService* pref_service,
                             const std::string& pref_name,
                             const std::string& generic_family,
                             const std::string& script,
                             bool incognito);

  // Dispatches the setting changed event |event_name| to extensions. The new
  // value of the setting is the value of browser pref |pref_name|. This value
  // is passed in the JavaScript object argument to the extension event function
  // under the key |key|. If the pref changed on the incognito profile,
  // |incognito| must be set to true for extensions to get the appropriate
  // event.
  void OnFontPrefChanged(PrefService* pref_service,
                         const std::string& pref_name,
                         const std::string& event_name,
                         const std::string& key,
                         bool incognito);

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;

  // Maps browser pref names to extension API <event name, key> pairs.
  PrefEventMap pref_event_map_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFontSettingsEventRouter);
};

// fontSettings.clearFont API function.
class ClearFontFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.clearFont")

 protected:
  // RefCounted types have non-public destructors, as with all extension
  // functions in this file.
  virtual ~ClearFontFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// fontSettings.getFont API function.
class GetFontFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.getFont")

 protected:
  virtual ~GetFontFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// fontSettings.setFont API function.
class SetFontFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.setFont")

 protected:
  virtual ~SetFontFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// fontSettings.getFontList API function.
class GetFontListFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.getFontList")

 protected:
  virtual ~GetFontListFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void FontListHasLoaded(scoped_ptr<base::ListValue> list);
  bool CopyFontsToResult(base::ListValue* fonts);
};

// Base class for extension API functions that clear a browser font pref.
class ClearFontPrefExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~ClearFontPrefExtensionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the preference to clear, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;
};

// Base class for extension API functions that get a browser font pref.
class GetFontPrefExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~GetFontPrefExtensionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the preference to get, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;

  // Implementations should return the key for the value in the extension API,
  // like "pixelSize".
  virtual const char* GetKey() = 0;
};

// Base class for extension API functions that set a browser font pref.
class SetFontPrefExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~SetFontPrefExtensionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the preference to set, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;

  // Implementations should return the key for the value in the extension API,
  // like "pixelSize".
  virtual const char* GetKey() = 0;
};

// The following are get/set/clear API functions that act on a browser font
// pref.

class ClearDefaultFontSizeFunction : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearDefaultFontSize")

 protected:
  virtual ~ClearDefaultFontSizeFunction() {}

  // ClearFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetDefaultFontSizeFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFontSize")

 protected:
  virtual ~GetDefaultFontSizeFunction() {}

  // GetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetDefaultFontSizeFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFontSize")

 protected:
  virtual ~SetDefaultFontSizeFunction() {}

  // SetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class ClearDefaultFixedFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearDefaultFixedFontSize")

 protected:
  virtual ~ClearDefaultFixedFontSizeFunction() {}

  // ClearFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetDefaultFixedFontSizeFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFixedFontSize")

 protected:
  virtual ~GetDefaultFixedFontSizeFunction() {}

  // GetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetDefaultFixedFontSizeFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFixedFontSize")

 protected:
  virtual ~SetDefaultFixedFontSizeFunction() {}

  // SetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class ClearMinimumFontSizeFunction : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearMinimumFontSize")

 protected:
  virtual ~ClearMinimumFontSizeFunction() {}

  // ClearFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetMinimumFontSizeFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getMinimumFontSize")

 protected:
  virtual ~GetMinimumFontSizeFunction() {}

  // GetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetMinimumFontSizeFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setMinimumFontSize")

 protected:
  virtual ~SetMinimumFontSizeFunction() {}

  // SetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
