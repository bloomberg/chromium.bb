// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_INPUT_STREAM_H_
#define ANDROID_WEBVIEW_NATIVE_INPUT_STREAM_H_

#include "android_webview/browser/input_stream.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"

namespace net {
class IOBuffer;
}

namespace android_webview {

class InputStreamImpl : public InputStream {
 public:
  // Maximum size of |buffer_|.
  static const int kBufferSize;

  static const InputStreamImpl* FromInputStream(
      const InputStream* input_stream);

  // |stream| should be an instance of the InputStream Java class.
  // |stream| can't be null.
  InputStreamImpl(const base::android::JavaRef<jobject>& stream);
  virtual ~InputStreamImpl();

  // Gets the underlying Java object. Guaranteed non-NULL.
  const jobject jobj() const { return jobject_.obj(); }

  // InputStream implementation.
  virtual bool BytesAvailable(int* bytes_available) const OVERRIDE;
  virtual bool Skip(int64_t n, int64_t* bytes_skipped) OVERRIDE;
  virtual bool Read(net::IOBuffer* dest, int length, int* bytes_read) OVERRIDE;
 protected:
  // Parameterless constructor exposed for testing.
  InputStreamImpl();

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobject_;
  base::android::ScopedJavaGlobalRef<jbyteArray> buffer_;

  DISALLOW_COPY_AND_ASSIGN(InputStreamImpl);
};

bool RegisterInputStream(JNIEnv* env);

} // namespace android_webview

#endif //  ANDROID_WEBVIEW_NATIVE_INPUT_STREAM_H_
