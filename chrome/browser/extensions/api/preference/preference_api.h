// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__

#include <string>

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/event_router.h"
#include "content/public/browser/notification_observer.h"
#include "extensions/browser/extension_prefs_scope.h"

class ExtensionPrefValueMap;
class PrefService;

namespace base {
class Value;
}

namespace extensions {
class ExtensionPrefs;

class PreferenceEventRouter {
 public:
  explicit PreferenceEventRouter(Profile* profile);
  virtual ~PreferenceEventRouter();

 private:
  void OnPrefChanged(PrefService* pref_service,
                     const std::string& pref_key);

  PrefChangeRegistrar registrar_;
  PrefChangeRegistrar incognito_registrar_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceEventRouter);
};

// The class containing the implementation for extension-controlled preference
// manipulation. This implementation is separate from PreferenceAPI, since
// we need to be able to use these methods in testing, where we use
// TestExtensionPrefs and don't construct a profile.
//
// See also PreferenceAPI and TestPreferenceAPI.
class PreferenceAPIBase {
 public:
  // Functions for manipulating preference values that are controlled by the
  // extension. In other words, these are not pref values *about* the extension,
  // but rather about something global the extension wants to override.

  // Set a new extension-controlled preference value.
  // Takes ownership of |value|.
  void SetExtensionControlledPref(const std::string& extension_id,
                                  const std::string& pref_key,
                                  ExtensionPrefsScope scope,
                                  base::Value* value);

  // Remove an extension-controlled preference value.
  void RemoveExtensionControlledPref(const std::string& extension_id,
                                     const std::string& pref_key,
                                     ExtensionPrefsScope scope);

  // Returns true if currently no extension with higher precedence controls the
  // preference.
  bool CanExtensionControlPref(const std::string& extension_id,
                               const std::string& pref_key,
                               bool incognito);

  // Returns true if extension |extension_id| currently controls the
  // preference. If |from_incognito| is not NULL, looks at incognito preferences
  // first, and |from_incognito| is set to true if the effective pref value is
  // coming from the incognito preferences, false if it is coming from the
  // normal ones.
  bool DoesExtensionControlPref(const std::string& extension_id,
                                const std::string& pref_key,
                                bool* from_incognito);

 protected:
  // Virtual for testing.
  virtual ExtensionPrefs* extension_prefs() = 0;
  virtual ExtensionPrefValueMap* extension_pref_value_map() = 0;
};

class PreferenceAPI : public PreferenceAPIBase,
                      public ProfileKeyedAPI,
                      public EventRouter::Observer,
                      public ContentSettingsStore::Observer {
 public:
  explicit PreferenceAPI(Profile* profile);
  virtual ~PreferenceAPI();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<PreferenceAPI>* GetFactoryInstance();

  // Convenience method to get the PreferenceAPI for a profile.
  static PreferenceAPI* Get(Profile* profile);

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

  // Loads the preferences controlled by the specified extension from their
  // dictionary and sets them in the |value_map|.
  static void LoadExtensionControlledPrefs(ExtensionPrefs* prefs,
                                           ExtensionPrefValueMap* value_map,
                                           const std::string& extension_id,
                                           ExtensionPrefsScope scope);

  // Store extension controlled preference values in the |value_map|,
  // which then informs the subscribers (ExtensionPrefStores) about the winning
  // values.
  static void InitExtensionControlledPrefs(ExtensionPrefs* prefs,
                                           ExtensionPrefValueMap* value_map);

 private:
  friend class ProfileKeyedAPIFactory<PreferenceAPI>;

  // ContentSettingsStore::Observer implementation.
  virtual void OnContentSettingChanged(const std::string& extension_id,
                                       bool incognito) OVERRIDE;

  // Clears incognito session-only content settings for all extensions.
  void ClearIncognitoSessionOnlyContentSettings();

  // PreferenceAPIBase implementation.
  virtual ExtensionPrefs* extension_prefs() OVERRIDE;
  virtual ExtensionPrefValueMap* extension_pref_value_map() OVERRIDE;

  Profile* profile_;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "PreferenceAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<PreferenceEventRouter> preference_event_router_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceAPI);
};

class PrefTransformerInterface {
 public:
  virtual ~PrefTransformerInterface() {}

  // Converts the representation of a preference as seen by the extension
  // into a representation that is used in the pref stores of the browser.
  // Returns the pref store representation in case of success or sets
  // |error| and returns NULL otherwise. |bad_message| is passed to simulate
  // the behavior of EXTENSION_FUNCTION_VALIDATE. It is never NULL.
  // The ownership of the returned value is passed to the caller.
  virtual base::Value* ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) = 0;

  // Converts the representation of the preference as stored in the browser
  // into a representation that is used by the extension.
  // Returns the extension representation in case of success or NULL otherwise.
  // The ownership of the returned value is passed to the caller.
  virtual base::Value* BrowserToExtensionPref(
      const base::Value* browser_pref) = 0;
};

// A base class to provide functionality common to the other *PreferenceFunction
// classes.
class PreferenceFunction : public ChromeSyncExtensionFunction {
 protected:
  virtual ~PreferenceFunction();

  // Given an |extension_pref_key|, provides its |browser_pref_key| from the
  // static map in preference_api.cc. Returns true if the corresponding
  // browser pref exists and the extension has the API permission needed to
  // modify that pref. Sets |error_| if the extension doesn't have the needed
  // permission.
  bool ValidateBrowserPref(const std::string& extension_pref_key,
                           std::string* browser_pref_key);
};

class GetPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.get", TYPES_CHROMESETTING_GET)

 protected:
  virtual ~GetPreferenceFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.set", TYPES_CHROMESETTING_SET)

 protected:
  virtual ~SetPreferenceFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ClearPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.clear",
                             TYPES_CHROMESETTING_CLEAR)

 protected:
  virtual ~ClearPreferenceFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__
