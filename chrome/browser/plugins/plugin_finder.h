// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "webkit/plugins/webplugininfo.h"

namespace base {
class DictionaryValue;
}

class GURL;
class PluginMetadata;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
#endif

// This class should be created and initialized by calling
// |GetInstance()| and |Init()| on the UI thread.
// After that it can be safely used on any other thread.
class PluginFinder {
 public:
  static PluginFinder* GetInstance();

  // It should be called on the UI thread.
  void Init();

  // TODO(ibraaaa): DELETE. http://crbug.com/124396
  static void Get(const base::Callback<void(PluginFinder*)>& cb);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US) and returns the PluginInstaller for the plug-in,
  // or NULL if no plug-in is found.
  PluginInstaller* FindPlugin(const std::string& mime_type,
                              const std::string& language);

  // Returns the plug-in with the given identifier.
  PluginInstaller* FindPluginWithIdentifier(const std::string& identifier);
#endif

  // Returns the plug-in metadata with the given identifier.
  PluginMetadata* FindPluginMetadataWithIdentifier(
      const std::string& identifier);

  // Gets plug-in metadata using |plugin|.
  PluginMetadata* GetPluginMetadata(const webkit::WebPluginInfo& plugin);

 private:
  friend struct DefaultSingletonTraits<PluginFinder>;
  friend class Singleton<PluginFinder>;
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, JsonSyntax);
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, PluginGroups);

  PluginFinder();
  ~PluginFinder();

  // Loads the plug-in information from the browser resources and parses it.
  // Returns NULL if the plug-in list couldn't be parsed.
  static base::DictionaryValue* LoadPluginList();

  PluginMetadata* CreatePluginMetadata(
      const std::string& identifier,
      const base::DictionaryValue* plugin_dict);

  // Initialized in |Init()| method and is read-only after that.
  // No need to be synchronized.
  scoped_ptr<base::DictionaryValue> plugin_list_;
#if defined(ENABLE_PLUGIN_INSTALLATION)
  std::map<std::string, PluginInstaller*> installers_;
#endif

  std::map<std::string, PluginMetadata*> identifier_plugin_;

  // Note: Don't free memory for |name_plugin_| values
  // since it holds pointers to same instances
  // in |identifier_plugin_| (Double De-allocation).
  std::map<string16, PluginMetadata*> name_plugin_;

  // Synchronization for |installers_|, |identifier_plugin_| and
  // |name_plugin_| are required since multiple threads
  // can be accessing them concurrently.
  base::Lock mutex_;

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
