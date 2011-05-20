// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_STORE_H_

#include <list>
#include <map>
#include <string>

#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/common/content_settings.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class ListValue;

// This class is the backend for extension-defined content settings. It is used
// by the content_settings::ExtensionProvider to integrate its settings into the
// HostContentSettingsMap and by the content settings extension API to provide
// extensions with access to content settings.
class ExtensionContentSettingsStore {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a content setting changes in the
    // ExtensionContentSettingsStore.
    virtual void OnContentSettingChanged(
        const std::string& extension_id,
        bool incognito) = 0;

    // Called when the ExtensionContentSettingsStore is being destroyed, so
    // observers can invalidate their weak references.
    virtual void OnDestruction() = 0;
  };

  ExtensionContentSettingsStore();
  virtual ~ExtensionContentSettingsStore();

  // //////////////////////////////////////////////////////////////////////////

  // Sets the content |setting| for |pattern| of extension |ext_id|. The
  // |incognito| flag allow to set whether the provided setting is for
  // incognito mode only.
  // Precondition: the extension must be registered.
  // This method should only be called on the UI thread.
  void SetExtensionContentSetting(
      const std::string& ext_id,
      const ContentSettingsPattern& embedded_pattern,
      const ContentSettingsPattern& top_level_pattern,
      ContentSettingsType type,
      const content_settings::ResourceIdentifier& identifier,
      ContentSetting setting,
      bool incognito);

  ContentSetting GetEffectiveContentSetting(
      const GURL& embedded_url,
      const GURL& top_level_url,
      ContentSettingsType type,
      const content_settings::ResourceIdentifier& identifier,
      bool incognito) const;

  // Returns a list of all content setting rules for the content type |type|
  // and the resource identifier (if specified and the content type uses
  // resource identifiers).
  void GetContentSettingsForContentType(
      ContentSettingsType type,
      const content_settings::ResourceIdentifier& identifier,
      bool incognito,
      content_settings::ProviderInterface::Rules* rules) const;

  // Serializes all content settings set by the extension with ID |extension_id|
  // and returns them as a ListValue. The caller takes ownership of the returned
  // value.
  ListValue* GetSettingsForExtension(const std::string& extension_id) const;

  // Deserializes content settings rules from |list| and applies them as set by
  // the extension with ID |extension_id|.
  void SetExtensionContentSettingsFromList(const std::string& extension_id,
                                           const ListValue* dict);

  // //////////////////////////////////////////////////////////////////////////

  // Registers the time when an extension |ext_id| is installed.
  void RegisterExtension(const std::string& ext_id,
                         const base::Time& install_time,
                         bool is_enabled);

  // Deletes all entries related to extension |ext_id|.
  void UnregisterExtension(const std::string& ext_id);

  // Hides or makes the extension content settings of the specified extension
  // visible.
  void SetExtensionState(const std::string& ext_id, bool is_enabled);

  // Adds |observer|. This method should only be called on the UI thread.
  void AddObserver(Observer* observer);

  // Remove |observer|. This method should only be called on the UI thread.
  void RemoveObserver(Observer* observer);

 private:
  struct ExtensionEntry;
  struct ContentSettingSpec {
    ContentSettingSpec(const ContentSettingsPattern& pattern,
                       const ContentSettingsPattern& embedder_pattern,
                       ContentSettingsType type,
                       const content_settings::ResourceIdentifier& identifier,
                       ContentSetting setting);

    ContentSettingsPattern embedded_pattern;
    ContentSettingsPattern top_level_pattern;
    ContentSettingsType content_type;
    content_settings::ResourceIdentifier resource_identifier;
    ContentSetting setting;
  };

  typedef std::map<std::string, ExtensionEntry*> ExtensionEntryMap;

  typedef std::list<ContentSettingSpec> ContentSettingSpecList;

  ContentSetting GetContentSettingFromSpecList(
      const GURL& embedded_url,
      const GURL& top_level_url,
      ContentSettingsType type,
      const content_settings::ResourceIdentifier& identifier,
      const ContentSettingSpecList& setting_spec_list) const;

  ContentSettingSpecList* GetContentSettingSpecList(
      const std::string& ext_id,
      bool incognito);

  const ContentSettingSpecList* GetContentSettingSpecList(
      const std::string& ext_id,
      bool incognito) const;

  void NotifyOfContentSettingChanged(const std::string& extension_id,
                                     bool incognito);

  void NotifyOfDestruction();

  bool OnCorrectThread();

  ExtensionEntryMap entries_;

  ObserverList<Observer, false> observers_;

  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContentSettingsStore);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_STORE_H_
