// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#pragma once

#include <map>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_change_registrar.h"

class ExtensionFontSettingsEventRouter : public content::NotificationObserver {
 public:
  explicit ExtensionFontSettingsEventRouter(Profile* profile);
  virtual ~ExtensionFontSettingsEventRouter();

  void Init();

 private:
  typedef std::pair<std::string, std::string> EventAndKeyPair;
  // Map of pref name to a pair of event name and key (defined in the API) for
  // dispatching a pref changed event. For example,
  // "webkit.webprefs.default_font_size" to ("onDefaultFontSizedChanged",
  // "pixelSize").
  typedef std::map<std::string, EventAndKeyPair> PrefEventMap;

  void AddPrefToObserve(const char* pref_name,
                        const char* event_name,
                        const char* key);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnFontNamePrefChanged(PrefService* pref_service,
                             const std::string& pref_name,
                             const std::string& generic_family,
                             const std::string& script,
                             bool incognito);
  void OnFontPrefChanged(PrefService* pref_service,
                         const std::string& pref_name,
                         const std::string& event_name,
                         const std::string& key,
                         bool incognito);

  PrefChangeRegistrar registrar_;

  PrefEventMap pref_event_map_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFontSettingsEventRouter);
};

class ClearFontFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.clearFont")
};

class GetFontFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.getFont")
};

class SetFontFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.setFont")
};

class GetFontListFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.getFontList")

 private:
  void FontListHasLoaded(scoped_ptr<base::ListValue> list);
  bool CopyFontsToResult(base::ListValue* fonts);
};

// Base class for functions that clear a font pref.
class ClearFontPrefExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the preference to clear, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;
};

// Base class for functions that get a font pref.
class GetFontPrefExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the preference to get, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;

  // Implementations should return the key for the value in the extension API,
  // like "pixelSize".
  virtual const char* GetKey() = 0;
};

// Base class for functions that set a font pref.
class SetFontPrefExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the preference to set, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;

  // Implementations should return the key for the value in the extension API,
  // like "pixelSize".
  virtual const char* GetKey() = 0;
};

class ClearDefaultFontSizeFunction : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearDefaultFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetDefaultFontSizeFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetDefaultFontSizeFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class ClearDefaultFixedFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearDefaultFixedFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetDefaultFixedFontSizeFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFixedFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetDefaultFixedFontSizeFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFixedFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class ClearMinimumFontSizeFunction : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearMinimumFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetMinimumFontSizeFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getMinimumFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetMinimumFontSizeFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setMinimumFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class ClearDefaultCharacterSetFunction : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.clearDefaultCharacterSet")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetDefaultCharacterSetFunction : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultCharacterSet")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class SetDefaultCharacterSetFunction : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultCharacterSet")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
