// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_PREF_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_PREF_SERVICE_BRIDGE_H_

#include <jni.h>
#include <string>

#include "components/content_settings/core/common/content_settings.h"

class PrefServiceBridge {
 public:
  static bool RegisterPrefServiceBridge(JNIEnv* env);

  // Use |locale| to create a language-region pair and language code to prepend
  // to the default accept languages. For Malay, we'll end up creating
  // "ms-MY,ms,en-US,en", and for Swiss-German, we will have
  // "de-CH,de-DE,de,en-US,en".
  static void PrependToAcceptLanguagesIfNecessary(
      const std::string& locale,
      std::string* accept_languages);

  // Return the corresponding Android permission associated with the
  // ContentSettingsType specified (or an empty string if no permission exists).
  static std::string GetAndroidPermissionForContentSetting(
      ContentSettingsType content_type);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_PREF_SERVICE_BRIDGE_H_
