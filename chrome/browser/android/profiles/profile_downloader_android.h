// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROFILES_PROFILE_DOWNLOADER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PROFILES_PROFILE_DOWNLOADER_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/strings/string16.h"

class SkBitmap;

// Android wrapper of the ProfileDownloader which provides access from the Java
// layer. This class should only be accessed from the UI thread.
class ProfileDownloaderAndroid {
 public:
  // Registers the ProfileDownloaderAndroid's native methods through JNI.
  static bool Register(JNIEnv* env);

  static void OnProfileDownloadSuccess(
      const std::string& account_id_,
      const base::string16& full_name,
      const SkBitmap& bitmap);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileDownloaderAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PROFILES_PROFILE_DOWNLOADER_ANDROID_H_
