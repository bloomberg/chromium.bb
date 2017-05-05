// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_LIBRARY_LOADER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_LIBRARY_LOADER_H_

#include <jni.h>

namespace cronet {

jint CronetOnLoad(JavaVM* vm, void* reserved);
void CronetOnUnLoad(JavaVM* jvm, void* reserved);

// Returns true when running on initialization thread.
// Only callable after initialization thread is started.
bool OnInitThread();

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_LIBRARY_LOADER_H_
