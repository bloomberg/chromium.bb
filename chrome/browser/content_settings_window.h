// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_WINDOW_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_WINDOW_H_

class PrefService;
class Profile;

// A particular tab within the Content Settings dialog.  When passed to
// ShowContentSettingsWindow(), CONTENT_SETTINGS_TAB_DEFAULT means select the
// last.
enum ContentSettingsTab {
  CONTENT_SETTINGS_TAB_DEFAULT = -1,
  CONTENT_SETTINGS_TAB_COOKIES,
  CONTENT_SETTINGS_TAB_IMAGES,
  CONTENT_SETTINGS_TAB_JAVASCRIPT,
  CONTENT_SETTINGS_TAB_PLUGINS,
  CONTENT_SETTINGS_PAGE_POPUPS,
  CONTENT_SETTINGS_NUM_TABS
};

class ContentSettings {
 public:
  // Show the Content Settings window selecting the specified page.
  // If a Content Settings window is currently open, this just activates it
  // instead of opening a new one.
  static void ShowContentSettingsWindow(ContentSettingsTab page,
                                        Profile* profile);

  static void RegisterPrefs(PrefService* prefs);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_WINDOW_H_

