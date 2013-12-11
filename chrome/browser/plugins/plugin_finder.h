// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "content/public/common/webplugininfo.h"

namespace base {
class DictionaryValue;
}

class GURL;
class PluginMetadata;
class PrefRegistrySimple;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
#endif

// This class should be created and initialized by calling
// |GetInstance()| and |Init()| on the UI thread.
// After that it can be safely used on any other thread.
class PluginFinder {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static PluginFinder* GetInstance();

  // It should be called on the UI thread.
  void Init();

  void ReinitializePlugins(const base::DictionaryValue* json_metadata);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US). If found, sets |installer| to the
  // corresponding PluginInstaller and |plugin_metadata| to a copy of the
  // corresponding PluginMetadata.
  bool FindPlugin(const std::string& mime_type,
                  const std::string& language,
                  PluginInstaller** installer,
                  scoped_ptr<PluginMetadata>* plugin_metadata);

  // Finds the plug-in with the given identifier. If found, sets |installer|
  // to the corresponding PluginInstaller and |plugin_metadata| to a copy
  // of the corresponding PluginMetadata. |installer| may be NULL.
  bool FindPluginWithIdentifier(const std::string& identifier,
                                PluginInstaller** installer,
                                scoped_ptr<PluginMetadata>* plugin_metadata);
#endif

  // Returns the plug-in name with the given identifier.
  base::string16 FindPluginNameWithIdentifier(const std::string& identifier);

  // Gets plug-in metadata using |plugin|.
  scoped_ptr<PluginMetadata> GetPluginMetadata(
      const content::WebPluginInfo& plugin);

 private:
  friend struct DefaultSingletonTraits<PluginFinder>;
  friend class Singleton<PluginFinder>;
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, JsonSyntax);
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, PluginGroups);

  PluginFinder();
  ~PluginFinder();

  // Loads the plug-in information from the browser resources and parses it.
  // Returns NULL if the plug-in list couldn't be parsed.
  static base::DictionaryValue* LoadBuiltInPluginList();

#if defined(ENABLE_PLUGIN_INSTALLATION)
  std::map<std::string, PluginInstaller*> installers_;
#endif

  std::map<std::string, PluginMetadata*> identifier_plugin_;

  // Version of the metadata information. We use this to consolidate multiple
  // sources (baked into resource and fetched from a URL), making sure that we
  // don't overwrite newer versions with older ones.
  int version_;

  // Synchronization for the above member variables is
  // required since multiple threads can be accessing them concurrently.
  base::Lock mutex_;

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
