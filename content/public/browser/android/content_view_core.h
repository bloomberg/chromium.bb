// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include <jni.h>

class GURL;

namespace content {

class WebContents;

// Native side of the ContentViewCore.java, which is the primary way of
// communicating with the native Chromium code on Android.  This is a
// public interface used by native code outside of the content module.
//
// TODO(jrg): this is a shell.  Upstream the rest.
//
// TODO(jrg): downstream, this class derives from
// base::SupportsWeakPtr<ContentViewCore>.  Issues raised in
// http://codereview.chromium.org/10536066/ make us want to rethink
// ownership issues.
// FOR THE MERGE (downstream), re-add derivation from
// base::SupportsWeakPtr<ContentViewCore> to keep everything else working
// until this issue is resolved.
// http://b/6666045
class ContentViewCore {
 public:
  virtual void Destroy(JNIEnv* env, jobject obj) = 0;

  static ContentViewCore* Create(JNIEnv* env, jobject obj,
                                 WebContents* web_contents);
  static ContentViewCore* GetNativeContentViewCore(JNIEnv* env, jobject obj);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  virtual int GetNativeImeAdapter(JNIEnv* env, jobject obj) = 0;
  virtual void SelectBetweenCoordinates(JNIEnv* env, jobject obj,
                                        jint x1, jint y1, jint x2, jint y2) = 0;

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;

 protected:
  virtual ~ContentViewCore() {};
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_VIEW_CORE_H_
