// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGIN_FINDER_H_
#pragma once

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string16.h"

namespace base {
class DictionaryValue;
class ListValue;
}

class GURL;
class PluginInstaller;

class PluginFinder {
 public:
  typedef base::Callback<void(PluginInstaller*)> FindPluginCallback;

  static PluginFinder* GetInstance();

  // Loads the plug-in information from the browser resources and parses it.
  // Returns NULL if the plug-in list couldn't be parsed.
  static scoped_ptr<base::ListValue> LoadPluginList();

  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US) and calls the callback with the PluginInstaller
  // for the plug-in, or NULL if no plug-in is found.
  void FindPlugin(const std::string& mime_type,
                  const std::string& language,
                  const FindPluginCallback& callback);

  // Finds the plug-in with the given identifier and calls the callback.
  void FindPluginWithIdentifier(const std::string& identifier,
                                const FindPluginCallback& callback);

 private:
  friend struct DefaultSingletonTraits<PluginFinder>;

  PluginFinder();
  ~PluginFinder();

  static base::ListValue* LoadPluginListInternal();

  PluginInstaller* CreateInstaller(const std::string& identifier,
                                   const base::DictionaryValue* plugin_dict);
  PluginInstaller* FindPluginInternal(const std::string& mime_type,
                                      const std::string& language);

  scoped_ptr<base::ListValue> plugin_list_;
  std::map<std::string, PluginInstaller*> installers_;

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGIN_FINDER_H_
