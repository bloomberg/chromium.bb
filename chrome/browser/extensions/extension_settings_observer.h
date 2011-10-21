// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_OBSERVER_H_
#pragma once

#include "chrome/browser/extensions/extension_setting_changes.h"

class Profile;

// Interface for classes that listen to changes to extension settings.
class ExtensionSettingsObserver {
 public:
  // Called when a list of settings have changed for an extension.
  //
  // |profile| is the profile of the event originator, or NULL if the event
  // didn't originate from a background page.  This is so that events can avoid
  // being sent back to the background page which made the change.  When the
  // settings API is exposed to content scripts, this will need to be changed.
  virtual void OnSettingsChanged(
      const Profile* profile,
      const std::string& extension_id,
      const ExtensionSettingChanges& changes) = 0;

 protected:
  virtual ~ExtensionSettingsObserver();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_OBSERVER_H_
