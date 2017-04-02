// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"

namespace media {

using JavaObjectPtr =
    std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>;

// Converts jbyteArray (byte[] in Java) into std::string.
std::string JavaBytesToString(JNIEnv* env, jbyteArray j_byte_array);

// A helper method to create a JavaObjectPtr.
JavaObjectPtr CreateJavaObjectPtr(jobject object);

}  // namespace media
