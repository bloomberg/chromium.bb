// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/sessions/session_id.h"

class Browser;
class GURL;
class Profile;

namespace chrome {

class SettingsWindowManagerObserver;

// Class for managing settings windows when --enable-settings-window is enabled.
// TODO(stevenjb): Remove flag comment if enabled by default.

class SettingsWindowManager {
 public:
  static SettingsWindowManager* GetInstance();

  void AddObserver(SettingsWindowManagerObserver* observer);
  void RemoveObserver(SettingsWindowManagerObserver* observer);

  // Shows a chrome:// page (e.g. Settings, About) in an an existing system
  // Browser window for |profile| or creates a new one.
  void ShowChromePageForProfile(Profile* profile, const GURL& gurl);

  // If a Browser settings window for |profile| has already been created,
  // returns it, otherwise returns NULL.
  Browser* FindBrowserForProfile(Profile* profile);

  // Returns true if |browser| is a settings window.
  bool IsSettingsBrowser(Browser* browser) const;

 private:
  friend struct DefaultSingletonTraits<SettingsWindowManager>;
  typedef std::map<Profile*, SessionID::id_type> ProfileSessionMap;

  SettingsWindowManager();
  ~SettingsWindowManager();

  ObserverList<SettingsWindowManagerObserver> observers_;
  ProfileSessionMap settings_session_map_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowManager);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_H_
