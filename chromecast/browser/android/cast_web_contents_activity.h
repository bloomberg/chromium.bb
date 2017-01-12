// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_CAST_WEB_CONTENTS_ACTIVITY_H_
#define CHROMECAST_BROWSER_ANDROID_CAST_WEB_CONTENTS_ACTIVITY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}

namespace chromecast {
namespace shell {

// Helper class to get members of the CastWebContentsActivity displaying a
// given web_contents. This class is lazily created through the Get function and
// will manage its own lifetime via SupportsUserData.
class CastWebContentsActivity : base::SupportsUserData::Data {
 public:
  static bool RegisterJni(JNIEnv* env);
  static CastWebContentsActivity* Get(content::WebContents* web_contents);

  base::android::ScopedJavaLocalRef<jobject> GetContentVideoViewEmbedder();
  void SetContentVideoViewEmbedder(
      const base::android::JavaRef<jobject>& content_video_view_embedder);

 private:
  explicit CastWebContentsActivity(content::WebContents* web_contents);
  ~CastWebContentsActivity() override;

  base::android::ScopedJavaGlobalRef<jobject> content_video_view_embedder_;

  DISALLOW_COPY_AND_ASSIGN(CastWebContentsActivity);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_CAST_WEB_CONTENTS_ACTIVITY_H_
