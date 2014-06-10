// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_H

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace android_webview {

class AwPermissionRequestDelegate;

// This class wraps a permission request, it works with PermissionRequestHandler
// and its' Java peer to represent the request to AwContentsClient.
// The specific permission request should implement the
// AwPermissionRequestDelegate interface, See MediaPermissionRequest.
class AwPermissionRequest {
 public:
  // The definition must synced with Android's
  // android.webkit.PermissionRequest.
  enum Resource {
    Geolocation = 1 << 0,
    VideoCapture = 1 << 1,
    AudioCapture = 1 << 2,
    ProtectedMediaId = 1 << 3,
  };

  // Take the ownership of |delegate|.
  AwPermissionRequest(scoped_ptr<AwPermissionRequestDelegate> delegate);
  virtual ~AwPermissionRequest();

  base::WeakPtr<AwPermissionRequest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Create and return Java peer.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaPeer();

  // Return the Java peer.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Invoked by Java peer when request is processed, |granted| indicates the
  // request was granted or not.
  void OnAccept(JNIEnv* env, jobject jcaller, jboolean granted);

  // Return the origin which initiated the request.
  const GURL& GetOrigin();

  // Return the resources origin requested.
  int64 GetResources();

 private:
  friend class TestAwPermissionRequest;

  scoped_ptr<AwPermissionRequestDelegate> delegate_;
  JavaObjectWeakGlobalRef java_ref_;

  base::WeakPtrFactory<AwPermissionRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwPermissionRequest);
};

bool RegisterAwPermissionRequest(JNIEnv* env);

}  // namespace android_webivew

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_H
