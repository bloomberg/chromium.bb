// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_ANDROID_RUN_ANDROID_APPLICATION_FUNCTION_H_
#define MOJO_SHELL_STANDALONE_ANDROID_RUN_ANDROID_APPLICATION_FUNCTION_H_

#include "base/android/jni_android.h"
#include "base/files/file_path.h"

namespace mojo {
namespace shell {

// Type of the function that we inject from the main .so of the Mojo shell to
// the helper libbootstrap.so. This function will set the thunks in the
// application .so and call into application MojoMain. Injecting the function
// from the main .so ensures that the thunks are set correctly.

typedef void (*RunAndroidApplicationFn)(JNIEnv* env,
                                        jobject j_context,
                                        const base::FilePath& app_path,
                                        jint j_handle);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_ANDROID_RUN_ANDROID_APPLICATION_FUNCTION_H_
