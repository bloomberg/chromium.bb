// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/android/blimp_jni_registrar.h"

#include "base/android/jni_registrar.h"

namespace blimp {
namespace client {

// This method is declared in
// //blimp/client/public/android/blimp_jni_registrar.h, and either this function
// or the one in //blimp/client/core/android/blimp_jni_registrar.cc
// should be linked in to any binary blimp::client::RegisterBlimpJni.
bool RegisterBlimpJni(JNIEnv* env) {
  return true;
}

}  // namespace client
}  // namespace blimp
