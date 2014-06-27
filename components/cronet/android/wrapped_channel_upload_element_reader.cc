// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/wrapped_channel_upload_element_reader.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace {
jclass g_class_readablebytechannel;
jclass g_class_channel;
jmethodID g_method_read;
jmethodID g_method_close;
}  //namespace

namespace cronet {

bool WrappedChannelRegisterJni(JNIEnv* env) {
  g_class_readablebytechannel = static_cast<jclass>(env->NewGlobalRef(
      env->FindClass("java/nio/channels/ReadableByteChannel")));
  // TODO(mef): Per previous discussions the best way to do this is to have a
  // cronet-specific java wrapper that exposes necessary methods annotated as
  // @AccessedByNative and use jni generator to generate a wrapper method for
  // that.
  g_method_read = env->GetMethodID(
      g_class_readablebytechannel, "read", "(Ljava/nio/ByteBuffer;)I");

  // Due to a bug in the version of ART that shipped with 4.4, we're looking up
  // the close() method on the base interface. See b/15651032 for details.
  g_class_channel = static_cast<jclass>(env->NewGlobalRef(
      env->FindClass("java/nio/channels/Channel")));
  g_method_close = env->GetMethodID(g_class_channel, "close", "()V");
  if (!g_class_readablebytechannel || !g_method_read || !g_method_close) {
    return false;
  }
  return true;
}

WrappedChannelElementReader::WrappedChannelElementReader(JNIEnv* env,
                                                         jobject channel,
                                                         uint64 length)
    : length_(length), offset_(0) {
  channel_ = env->NewGlobalRef(channel);
}

WrappedChannelElementReader::~WrappedChannelElementReader() {
  JNIEnv* env = base::android::AttachCurrentThread();
  env->DeleteGlobalRef(channel_);
}

int WrappedChannelElementReader::Init(const net::CompletionCallback& callback) {
  offset_ = 0;
  return net::OK;
}

uint64 WrappedChannelElementReader::GetContentLength() const {
  return length_;
}

uint64 WrappedChannelElementReader::BytesRemaining() const {
  return length_ - offset_;
}

bool WrappedChannelElementReader::IsInMemory() const {
  return false;
}

int WrappedChannelElementReader::Read(net::IOBuffer* buf,
                                      int buf_length,
                                      const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject jbuf = env->NewDirectByteBuffer(reinterpret_cast<void*>(buf->data()),
                                          buf_length);
  jint bytes_read = env->CallIntMethod(channel_, g_method_read, jbuf);
  base::android::CheckException(env);

  env->DeleteLocalRef(jbuf);
  if (bytes_read <= 0) {
    env->CallVoidMethod(channel_, g_method_close);
    base::android::CheckException(env);
    return bytes_read;
  }
  offset_ += bytes_read;
  return bytes_read;
}

}  // namespace cronet

