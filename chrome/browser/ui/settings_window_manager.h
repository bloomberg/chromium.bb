// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/sessions/session_id.h"

class Profile;

namespace chrome {

// Class for managing settings windows when --enable-settings-window is enabled.
// TODO(stevenjb): Remove flag comment if enabled by default.

class SettingsWindowManager {
 public:
  static SettingsWindowManager* GetInstance();

  // Show an existing settings window for |profile| or create a new one, and
  // navigate to |sub_page|.
  void ShowForProfile(Profile* profile, const std::string& sub_page);

 private:
  friend struct DefaultSingletonTraits<SettingsWindowManager>;
  typedef std::map<Profile*, SessionID::id_type> ProfileSessionMap;

  SettingsWindowManager();
  ~SettingsWindowManager();

  ProfileSessionMap settings_session_map_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowManager);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_H_
