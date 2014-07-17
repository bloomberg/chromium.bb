// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
class ListValue;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
}

namespace options {

// Font settings overlay page UI handler.
class FontSettingsHandler : public OptionsPageUIHandler,
                            public extensions::ExtensionRegistryObserver {
 public:
  FontSettingsHandler();
  virtual ~FontSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;

 private:
  void HandleFetchFontsData(const base::ListValue* args);

  void FontsListHasLoaded(scoped_ptr<base::ListValue> list);

  void SetUpStandardFontSample();
  void SetUpSerifFontSample();
  void SetUpSansSerifFontSample();
  void SetUpFixedFontSample();
  void SetUpMinimumFontSample();

  // Returns the Advanced Font Settings Extension if it's installed and enabled,
  // or NULL otherwise.
  const extensions::Extension* GetAdvancedFontSettingsExtension();
  // Notifies the web UI about whether the Advanced Font Settings Extension is
  // installed and enabled.
  void NotifyAdvancedFontSettingsAvailability();
  // Opens the options page of the Advanced Font Settings Extension.
  void HandleOpenAdvancedFontSettingsOptions(const base::ListValue* args);

  void OnWebKitDefaultFontSizeChanged();

  StringPrefMember standard_font_;
  StringPrefMember serif_font_;
  StringPrefMember sans_serif_font_;
  StringPrefMember fixed_font_;
  StringPrefMember font_encoding_;
  IntegerPrefMember default_font_size_;
  IntegerPrefMember default_fixed_font_size_;
  IntegerPrefMember minimum_font_size_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(FontSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
