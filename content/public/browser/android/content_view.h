// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_VIEW_H_
#pragma once

#include <jni.h>

namespace content {
// Native side of the ContentView.java, the primary FrameLayout of
// Chromium on Android.  This is a public interface used by native
// code outside of the content module.
//
// TODO(jrg): this is a shell.  Upstream the rest.
//
// TODO(jrg): downstream, this class derives from
// base::SupportsWeakPtr<ContentView>.  Issues raised in
// http://codereview.chromium.org/10536066/ make us want to rethink
// ownership issues.
// FOR THE MERGE (downstream), re-add derivation from
// base::SupportsWeakPtr<ContentView> to keep everything else working
// until this issue is resolved.
// http://b/6666045
class ContentView {
 public:
  virtual void Destroy(JNIEnv* env, jobject obj) = 0;

  static ContentView* Create(JNIEnv* env, jobject obj);
  static ContentView* GetNativeContentView(JNIEnv* env, jobject obj);

 protected:
  virtual ~ContentView() {};
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_VIEW_H_
