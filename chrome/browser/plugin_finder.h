// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGIN_FINDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "webkit/plugins/webplugininfo.h"

namespace base {
class DictionaryValue;
}

class GURL;
class PluginInstaller;

class PluginFinder {
 public:
  typedef std::vector<webkit::WebPluginInfo> PluginVector;
  typedef base::Callback<void(const PluginVector&, PluginFinder*)>
      CombinedCallback;
  // Gets PluginFinder and vector of Plugins asynchronously. It then
  // calls |cb| once both are fetched.
  static void GetPluginsAndPluginFinder(const CombinedCallback& cb);

  static void Get(const base::Callback<void(PluginFinder*)>& cb);

  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US) and returns the PluginInstaller for the plug-in,
  // or NULL if no plug-in is found.
  PluginInstaller* FindPlugin(const std::string& mime_type,
                              const std::string& language);

  // Returns the plug-in with the given identifier.
  PluginInstaller* FindPluginWithIdentifier(const std::string& identifier);

  // Gets a plug-in installer using |plugin|.
  PluginInstaller* GetPluginInstaller(const webkit::WebPluginInfo& plugin);

 private:
  friend struct DefaultSingletonTraits<PluginFinder>;
  friend class Singleton<PluginFinder>;
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, JsonSyntax);
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, PluginGroups);

  static PluginFinder* GetInstance();

  PluginFinder();
  ~PluginFinder();

  // Loads the plug-in information from the browser resources and parses it.
  // Returns NULL if the plug-in list couldn't be parsed.
  static base::DictionaryValue* LoadPluginList();

  PluginInstaller* CreateInstaller(const std::string& identifier,
                                   const base::DictionaryValue* plugin_dict);

  scoped_ptr<base::DictionaryValue> plugin_list_;
  std::map<std::string, PluginInstaller*> installers_;

  // Note: Don't free memory for |name_insallers_| values
  // since it holds pointers to same instances
  // in |installers_| (Double De-allocation).
  base::hash_map<string16, PluginInstaller*> name_installers_;

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGIN_FINDER_H_
