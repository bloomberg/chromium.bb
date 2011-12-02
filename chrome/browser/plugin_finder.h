// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
class ListValue;
}

class GURL;
class PluginInstaller;

class PluginFinder {
 public:
  typedef base::Callback<void(PluginInstaller*)> FindPluginCallback;

  static PluginFinder* GetInstance();

  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US) and calls one of the two passed in callbacks,
  // depending on whether a plug-in is found.
  void FindPlugin(const std::string& mime_type,
                  const std::string& language,
                  const FindPluginCallback& found_callback,
                  const base::Closure& not_found_callback);

 private:
  friend struct DefaultSingletonTraits<PluginFinder>;

  PluginFinder();
  ~PluginFinder();

  scoped_ptr<base::ListValue> plugin_list_;
  std::map<std::string, PluginInstaller*> installers_;

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGIN_FINDER_H_
