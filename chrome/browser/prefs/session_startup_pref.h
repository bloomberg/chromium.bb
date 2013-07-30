// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_SESSION_STARTUP_PREF_H__
#define CHROME_BROWSER_PREFS_SESSION_STARTUP_PREF_H__

#include <vector>

#include "url/gurl.h"

class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// StartupPref specifies what should happen at startup for a specified profile.
// StartupPref is stored in the preferences for a particular profile.
struct SessionStartupPref {
  enum Type {
    // Indicates the user wants to open the New Tab page.
    DEFAULT,

    // Deprecated. See comment in session_startup_pref.cc
    HOMEPAGE,

    // Indicates the user wants to restore the last session.
    LAST,

    // Indicates the user wants to restore a specific set of URLs. The URLs
    // are contained in urls.
    URLS,

    // Number of values in this enum.
    TYPE_COUNT
  };

  // For historical reasons the enum and value registered in the prefs don't
  // line up. These are the values registered in prefs.
  // The values are also recorded in Settings.StartupPageLoadSettings histogram,
  // so make sure to update histograms.xml if you change these.
  static const int kPrefValueHomePage = 0;  // Deprecated
  static const int kPrefValueLast = 1;
  static const int kPrefValueURLs = 4;
  static const int kPrefValueNewTab = 5;
  static const int kPrefValueMax = 6;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the default value for |type|.
  static Type GetDefaultStartupType();

  // What should happen on startup for the specified profile.
  static void SetStartupPref(Profile* profile, const SessionStartupPref& pref);
  static void SetStartupPref(PrefService* prefs,
                             const SessionStartupPref& pref);
  static SessionStartupPref GetStartupPref(Profile* profile);
  static SessionStartupPref GetStartupPref(PrefService* prefs);

  // If the user had the "restore on startup" property set to the deprecated
  // "Open the home page" value, this migrates them to a value that will have
  // the same effect.
  static void MigrateIfNecessary(PrefService* prefs);

  // The default startup pref for Mac used to be LAST, now it's DEFAULT. This
  // migrates old users by writing out the preference explicitly.
  static void MigrateMacDefaultPrefIfNecessary(PrefService* prefs);

  // Whether the startup type and URLs are managed via policy.
  static bool TypeIsManaged(PrefService* prefs);
  static bool URLsAreManaged(PrefService* prefs);

  // Whether the startup type has not been overridden from its default.
  static bool TypeIsDefault(PrefService* prefs);

  // Converts an integer pref value to a SessionStartupPref::Type.
  static SessionStartupPref::Type PrefValueToType(int pref_value);

  explicit SessionStartupPref(Type type);

  ~SessionStartupPref();

  // What to do on startup.
  Type type;

  // The URLs to restore. Only used if type == URLS.
  std::vector<GURL> urls;
};

#endif  // CHROME_BROWSER_PREFS_SESSION_STARTUP_PREF_H__
