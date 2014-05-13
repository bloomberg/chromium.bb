// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_EXTERNAL_PRERENDER_HANDLER_ANDROID_H_
#define CHROME_BROWSER_PRERENDER_EXTERNAL_PRERENDER_HANDLER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace prerender {
class PrerenderHandle;

// A class for handling external prerender requests from other applications.
class ExternalPrerenderHandlerAndroid {
 public:
  explicit ExternalPrerenderHandlerAndroid();

  // Add a prerender with the given url and referrer on the PrerenderManager of
  // the given profile. This is restricted to a single prerender at a time.
  bool AddPrerender(JNIEnv* env,
                    jobject obj,
                    jobject profile,
                    jlong web_content_ptr,
                    jstring url,
                    jstring referrer,
                    jint width,
                    jint height);

  // Cancel the prerender associated with the prerender_handle_
  void CancelCurrentPrerender(JNIEnv* env, jobject object);

  // Whether the PrerenderManager associated with the given profile has any
  // prerenders for the url.
  static bool HasPrerenderedUrl(Profile* profile,
                                GURL url,
                                content::WebContents* web_contents);

  // Whether the cookie store associated with this profile has been loaded.
  static bool HasCookieStoreLoaded(Profile* profile);

  static bool RegisterExternalPrerenderHandlerAndroid(JNIEnv* env);

 private:
  virtual ~ExternalPrerenderHandlerAndroid();
  scoped_ptr<prerender::PrerenderHandle> prerender_handle_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrerenderHandlerAndroid);
};

} // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_EXTERNAL_PRERENDER_HANDLER_ANDROID_H_
