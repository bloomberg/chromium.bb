// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PROXY_CONFIG_DICTIONARY_H_
#define CHROME_BROWSER_PREFS_PROXY_CONFIG_DICTIONARY_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/prefs/proxy_prefs.h"

class DictionaryValue;

// Factory and wrapper for proxy config dictionaries that are stored
// in the user preferences. The dictionary has the following structure:
// {
//   mode: string,
//   server: string,
//   pac_url: string,
//   bypass_list: string
// }
// See proxy_config_dictionary.cc for the structure of the respective strings.
class ProxyConfigDictionary {
 public:
  // Creates a deep copy of |dict| and leaves ownership to caller.
  explicit ProxyConfigDictionary(const DictionaryValue* dict);
  ~ProxyConfigDictionary();

  bool GetMode(ProxyPrefs::ProxyMode* out) const;
  bool GetPacUrl(std::string* out) const;
  bool GetProxyServer(std::string* out) const;
  bool GetBypassList(std::string* out) const;

  static DictionaryValue* CreateDirect();
  static DictionaryValue* CreateAutoDetect();
  static DictionaryValue* CreatePacScript(const std::string& pac_url);
  static DictionaryValue* CreateFixedServers(
      const std::string& proxy_server,
      const std::string& bypass_list);
  static DictionaryValue* CreateSystem();
 private:
  static DictionaryValue* CreateDictionary(ProxyPrefs::ProxyMode mode,
                                           const std::string& pac_url,
                                           const std::string& proxy_server,
                                           const std::string& bypass_list);

  scoped_ptr<DictionaryValue> dict_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigDictionary);
};

#endif  // CHROME_BROWSER_PREFS_PROXY_CONFIG_DICTIONARY_H_
