// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the classes to realize the Font Settings Extension API as specified
// in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

namespace extensions {

// This class observes pref changed events on a profile and dispatches the
// corresponding extension API events to extensions.
class FontSettingsEventRouter {
 public:
  // Constructor for observing pref changed events on |profile|. Stores a
  // pointer to |profile| but does not take ownership. |profile| must be
  // non-NULL and remain alive for the lifetime of the instance.
  explicit FontSettingsEventRouter(Profile* profile);
  virtual ~FontSettingsEventRouter();

 private:
  // Observes browser pref |pref_name|. When a change is observed, dispatches
  // event |event_name| to extensions. A JavaScript object is passed to the
  // extension event function with the new value of the pref in property |key|.
  void AddPrefToObserve(const char* pref_name,
                        const char* event_name,
                        const char* key);

  // Decodes a preference change for a font family map and invokes
  // OnFontNamePrefChange with the right parameters.
  void OnFontFamilyMapPrefChanged(const std::string& pref_name);

  // Dispatches a changed event for the font setting for |generic_family| and
  // |script| to extensions. The new value of the setting is the value of
  // browser pref |pref_name|.
  void OnFontNamePrefChanged(const std::string& pref_name,
                             const std::string& generic_family,
                             const std::string& script);

  // Dispatches the setting changed event |event_name| to extensions. The new
  // value of the setting is the value of browser pref |pref_name|. This value
  // is passed in the JavaScript object argument to the extension event function
  // under the key |key|.
  void OnFontPrefChanged(const std::string& event_name,
                         const std::string& key,
                         const std::string& pref_name);

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FontSettingsEventRouter);
};

// The profile-keyed service that manages the font_settings extension API.
// This is not an EventRouter::Observer (and does not lazily initialize) because
// doing so caused a regression in perf tests. See crbug.com/163466.
class FontSettingsAPI : public ProfileKeyedService {
 public:
  explicit FontSettingsAPI(Profile* profile);
  virtual ~FontSettingsAPI();

 private:
  scoped_ptr<FontSettingsEventRouter> font_settings_event_router_;
};

// fontSettings.clearFont API function.
class FontSettingsClearFontFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.clearFont")

 protected:
  // RefCounted types have non-public destructors, as with all extension
  // functions in this file.
  virtual ~FontSettingsClearFontFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// fontSettings.getFont API function.
class FontSettingsGetFontFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.getFont")

 protected:
  virtual ~FontSettingsGetFontFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// fontSettings.setFont API function.
class FontSettingsSetFontFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.setFont")

 protected:
  virtual ~FontSettingsSetFontFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// fontSettings.getFontList API function.
class FontSettingsGetFontListFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.getFontList")

 protected:
  virtual ~FontSettingsGetFontListFunction() {}

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

class FontSettingsClearDefaultFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.clearDefaultFontSize")

 protected:
  virtual ~FontSettingsClearDefaultFontSizeFunction() {}

  // ClearFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
};

class FontSettingsGetDefaultFontSizeFunction
    : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.getDefaultFontSize")

 protected:
  virtual ~FontSettingsGetDefaultFontSizeFunction() {}

  // GetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class FontSettingsSetDefaultFontSizeFunction
    : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.setDefaultFontSize")

 protected:
  virtual ~FontSettingsSetDefaultFontSizeFunction() {}

  // SetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class FontSettingsClearDefaultFixedFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.clearDefaultFixedFontSize")

 protected:
  virtual ~FontSettingsClearDefaultFixedFontSizeFunction() {}

  // ClearFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
};

class FontSettingsGetDefaultFixedFontSizeFunction
    : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.getDefaultFixedFontSize")

 protected:
  virtual ~FontSettingsGetDefaultFixedFontSizeFunction() {}

  // GetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class FontSettingsSetDefaultFixedFontSizeFunction
    : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.setDefaultFixedFontSize")

 protected:
  virtual ~FontSettingsSetDefaultFixedFontSizeFunction() {}

  // SetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class FontSettingsClearMinimumFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.clearMinimumFontSize")

 protected:
  virtual ~FontSettingsClearMinimumFontSizeFunction() {}

  // ClearFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
};

class FontSettingsGetMinimumFontSizeFunction
    : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.getMinimumFontSize")

 protected:
  virtual ~FontSettingsGetMinimumFontSizeFunction() {}

  // GetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

class FontSettingsSetMinimumFontSizeFunction
    : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fontSettings.setMinimumFontSize")

 protected:
  virtual ~FontSettingsSetMinimumFontSizeFunction() {}

  // SetFontPrefExtensionFunction:
  virtual const char* GetPrefName() OVERRIDE;
  virtual const char* GetKey() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_
