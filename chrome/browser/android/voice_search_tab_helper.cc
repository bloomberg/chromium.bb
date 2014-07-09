// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/voice_search_tab_helper.h"

#include "components/google/core/browser/google_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "jni/VoiceSearchTabHelper_jni.h"

using content::WebContents;

// Register native methods
bool RegisterVoiceSearchTabHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void UpdateAutoplayStatus(JNIEnv* env,
                                 jobject obj,
                                 jobject j_web_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(j_web_contents);
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  content::WebPreferences prefs = host->GetWebkitPreferences();

  // In the case where media autoplay has been enabled by default (e.g. in
  // performance media tests) do not update it based on navigation changes.
  //
  // Note that GetWekitPreferences() is 'stateless'. It returns the default
  // webkit preferences configuration from command line switches.
  if (!prefs.user_gesture_required_for_media_playback)
    return;

  prefs.user_gesture_required_for_media_playback =
      !google_util::IsGoogleSearchUrl(web_contents->GetLastCommittedURL());
  host->UpdateWebkitPreferences(prefs);
}
