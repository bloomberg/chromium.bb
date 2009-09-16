// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OUTDATED_PLUGINS_H
#define CHROME_BROWSER_OUTDATED_PLUGINS_H

#include <vector>

struct WebPluginInfo;
class MessageLoop;

class OutdatedPlugins {
 public:
  // Check a list of plugins and (asynchronously) throw up info bars for those
  // which we consider to be outdated.
  static void Check(const std::vector<WebPluginInfo>& plugins,
                    MessageLoop* ui_loop);

  // Version check functions: exported for testing
  static bool CheckFlashVersion(const std::wstring& version);

 private:
  static void PluginAlert(std::vector<unsigned>* outdated_plugins);
};

#endif  // CHROME_BROWSER_OUTDATED_PLUGINS_H
