// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_UI_THUMBNAIL_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_UI_THUMBNAIL_PROVIDER_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/image_decoder.h"

// Kicks off asynchronous pipelines for creating thumbnails for local files.
// The native-side ThumbnailProvider is owned by the Java-side and can be
// safely destroyed while a request is being processed.
class ThumbnailProvider {
 public:
  explicit ThumbnailProvider(const base::android::JavaParamRef<jobject>& jobj);

  // Destroys the ThumbnailProvider.  Any currently running ImageRequest will
  // delete itself when it has completed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Kicks off an asynchronous process to retrieve the thumbnail for the file
  // located at |file_path| with a max size of |icon_size| in each dimension.
  void RetrieveThumbnail(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jobj,
                         jstring jfile_path,
                         jint icon_size);

  // Called when the thumbnail is ready.  |thumbnail| will be empty on failure.
  void OnThumbnailRetrieved(const std::string& file_path,
                            const SkBitmap& thumbnail);

  // Registers the JNI bindings.
  static bool RegisterThumbnailProvider(JNIEnv* env);

 private:
  ~ThumbnailProvider();

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::WeakPtrFactory<ThumbnailProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailProvider);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_UI_THUMBNAIL_PROVIDER_H_
