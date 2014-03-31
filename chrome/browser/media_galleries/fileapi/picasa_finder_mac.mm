// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa_finder.h"

#include "base/files/file_path.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "components/policy/core/common/preferences_mac.h"
#include "content/public/browser/browser_thread.h"

using base::mac::CFCast;
using base::mac::CFToNSCast;
using base::mac::NSToCFCast;

namespace picasa {

namespace {

static MacPreferences* g_test_mac_preferences = NULL;

}  // namespace

NSString* const kPicasaAppDataPathMacPreferencesKey = @"AppLocalDataPath";

void SetMacPreferencesForTesting(MacPreferences* preferences) {
  g_test_mac_preferences = preferences;
}

base::FilePath GetCustomPicasaAppDataPathFromMacPreferences() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  scoped_ptr<MacPreferences> real_preferences;
  MacPreferences* prefs = g_test_mac_preferences;
  if (!prefs) {
    real_preferences.reset(new MacPreferences());
    prefs = real_preferences.get();
  }

  CFStringRef picasa_app_id = CFSTR("com.google.picasa");
  base::scoped_nsobject<NSString> database_path(
      CFToNSCast(CFCast<CFStringRef>(prefs->CopyAppValue(
          NSToCFCast(kPicasaAppDataPathMacPreferencesKey), picasa_app_id))));

  if (database_path == NULL)
    return base::FilePath();

  return base::mac::NSStringToFilePath(database_path.get());
}

}  // namespace picasa
