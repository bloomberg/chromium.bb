// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_BACKUP_WATCHER_H_
#define CHROME_BROWSER_ANDROID_CHROME_BACKUP_WATCHER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace android {

// Watch the preferences that we back up using Android Backup, so that we can
// create a new backup whenever any of them change.
class ChromeBackupWatcher {
 public:
  explicit ChromeBackupWatcher(Profile* profile);
  virtual ~ChromeBackupWatcher();

 private:
  PrefChangeRegistrar registrar_;
  base::android::ScopedJavaGlobalRef<jobject> java_watcher_;
  DISALLOW_COPY_AND_ASSIGN(ChromeBackupWatcher);
};

}  //  namespace android
#endif  // CHROME_BROWSER_ANDROID_CHROME_BACKUP_WATCHER_H_
