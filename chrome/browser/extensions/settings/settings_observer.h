// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_OBSERVER_H_

#include "base/observer_list_threadsafe.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"

namespace extensions {

// Interface for classes that listen to changes to extension settings.
class SettingsObserver {
 public:
  // Called when a list of settings have changed for an extension.
  virtual void OnSettingsChanged(
      const std::string& extension_id,
      settings_namespace::Namespace settings_namespace,
      const std::string& changes_json) = 0;

  virtual ~SettingsObserver() {}
};

typedef ObserverListThreadSafe<SettingsObserver>
    SettingsObserverList;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_OBSERVER_H_
