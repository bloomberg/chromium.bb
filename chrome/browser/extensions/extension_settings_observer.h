// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_OBSERVER_H_
#pragma once

#include "base/observer_list_threadsafe.h"

// Interface for classes that listen to changes to extension settings.
class ExtensionSettingsObserver {
 public:
  // Called when a list of settings have changed for an extension.
  virtual void OnSettingsChanged(
      const std::string& extension_id,
      const std::string& changes_json) = 0;

 protected:
  virtual ~ExtensionSettingsObserver();
};

typedef ObserverListThreadSafe<ExtensionSettingsObserver>
    ExtensionSettingsObserverList;

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_OBSERVER_H_
