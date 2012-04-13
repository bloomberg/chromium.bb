// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGIN_FINDER_H_
#pragma once

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string16.h"

namespace base {
class DictionaryValue;
}

class GURL;
class PluginInstaller;

class PluginFinder {
 public:
  static void Get(const base::Callback<void(PluginFinder*)>& cb);

  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US) and returns the PluginInstaller for the plug-in,
  // or NULL if no plug-in is found.
  PluginInstaller* FindPlugin(const std::string& mime_type,
                              const std::string& language);

  // Returns the plug-in with the given identifier.
  PluginInstaller* FindPluginWithIdentifier(const std::string& identifier);

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

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGIN_FINDER_H_
