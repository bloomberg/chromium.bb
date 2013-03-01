// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_bridge.h"

#include <jni.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"

#include "jni/MediaCodec_jni.h"
#include "jni/MediaFormat_jni.h"


using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

static jclass g_MediaCodecBufferInfo_clazz = NULL;

static const char kMediaCodecBufferInfoClassPath[] =
    "android/media/MediaCodec$BufferInfo";

static bool MediaCodecBufferInfo_RegisterNativesImpl(JNIEnv* env) {
  g_MediaCodecBufferInfo_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kMediaCodecBufferInfoClassPath)));
  base::android::CheckException(env);
  return true;
}

static void GetBufferInfo(JNIEnv* env, jobject buffer_info, int* offset,
                          int* size, int64* presentation_time, int* flags) {
  static jfieldID offset_id =
      env->GetFieldID(g_MediaCodecBufferInfo_clazz, "offset", "I");
  static jfieldID size_id =
      env->GetFieldID(g_MediaCodecBufferInfo_clazz, "size", "I");
  static jfieldID presentation_time_id =
      env->GetFieldID(g_MediaCodecBufferInfo_clazz, "presentationTimeUs", "J");
  static jfieldID flags_id =
      env->GetFieldID(g_MediaCodecBufferInfo_clazz, "flags", "I");

  *offset = env->GetIntField(buffer_info, offset_id);
  *size = env->GetIntField(buffer_info, size_id);
  *presentation_time = env->GetLongField(buffer_info, presentation_time_id);
  *flags = env->GetIntField(buffer_info, flags_id);
}

static base::subtle::AtomicWord g_MediaCodecBufferInfo_Constructor = 0;
static ScopedJavaLocalRef<jobject> Java_MediaCodecBufferInfo_Constructor(
    JNIEnv* env) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_MediaCodecBufferInfo_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<base::android::MethodID::TYPE_INSTANCE>(
      env, g_MediaCodecBufferInfo_clazz,
      "<init>", "()V",
      &g_MediaCodecBufferInfo_Constructor);

  jobject ret = env->NewObject(g_MediaCodecBufferInfo_clazz, method_id);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

class MediaCodecNativeRegisterer {
 public:
  MediaCodecNativeRegisterer() {
    JNIEnv* env = AttachCurrentThread();
    jni_initialized_ =
        MediaCodecBufferInfo_RegisterNativesImpl(env) &&
        JNI_MediaCodec::RegisterNativesImpl(env) &&
        JNI_MediaFormat::RegisterNativesImpl(env);
  }

  bool IsRegistered() {
    return jni_initialized_;
  }

 private:
  bool jni_initialized_;
};

static base::LazyInstance<MediaCodecNativeRegisterer> g_native_registerer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace media {

enum { kBufferFlagEndOfStream = 4 };

static const char* CodecToMimeType(const MediaCodecBridge::Codec codec) {
  switch (codec) {
    case MediaCodecBridge::AUDIO_MPEG: return "audio/mpeg";
    case MediaCodecBridge::VIDEO_H264: return "video/avc";
    case MediaCodecBridge::VIDEO_VP8: return "video/x-vnd.on2.vp8";
    default: return NULL;
  }
}

// static
const base::TimeDelta MediaCodecBridge::kTimeOutInfinity =
    base::TimeDelta::FromMicroseconds(-1);

// static
const base::TimeDelta MediaCodecBridge::kTimeOutNoWait =
    base::TimeDelta::FromMicroseconds(0);

// static
bool MediaCodecBridge::IsAvailable() {
  // MediaCodec is only available on JB and greater.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 16;
}

MediaCodecBridge::MediaCodecBridge(const Codec codec) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  CHECK(g_native_registerer.Pointer()->IsRegistered());
  DCHECK(CodecToMimeType(codec));

  ScopedJavaLocalRef<jstring> j_type =
      ConvertUTF8ToJavaString(env, CodecToMimeType(codec));

  ScopedJavaLocalRef<jobject> tmp(
      JNI_MediaCodec::Java_MediaCodec_createDecoderByType(env, j_type.obj()));
  DCHECK(!tmp.is_null());
  j_media_codec_.Reset(tmp);
}

MediaCodecBridge::~MediaCodecBridge() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  JNI_MediaCodec::Java_MediaCodec_release(env, j_media_codec_.obj());
}

void MediaCodecBridge::StartAudio(
    const Codec codec, int sample_rate, int channel_count) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(CodecToMimeType(codec));

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, CodecToMimeType(codec));
  ScopedJavaLocalRef<jobject> j_format(
      JNI_MediaFormat::Java_MediaFormat_createAudioFormat(
          env, j_mime.obj(), sample_rate, channel_count));
  DCHECK(!j_format.is_null());

  JNI_MediaCodec::Java_MediaCodec_configure(
      env, j_media_codec_.obj(), j_format.obj(), NULL, NULL, 0);

  Start();
}

void MediaCodecBridge::StartVideo(
    const Codec codec, const gfx::Size& size, jobject surface) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(CodecToMimeType(codec));

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, CodecToMimeType(codec));
  ScopedJavaLocalRef<jobject> j_format(
      JNI_MediaFormat::Java_MediaFormat_createVideoFormat(
          env, j_mime.obj(), size.width(), size.height()));
  DCHECK(!j_format.is_null());

  JNI_MediaCodec::Java_MediaCodec_configure(
      env, j_media_codec_.obj(), j_format.obj(), surface, NULL, 0);

  Start();
}

void MediaCodecBridge::Start() {
  JNIEnv* env = AttachCurrentThread();
  JNI_MediaCodec::Java_MediaCodec_start(env, j_media_codec_.obj());
  GetInputBuffers();
}

void MediaCodecBridge::Reset() {
  JNIEnv* env = AttachCurrentThread();
  JNI_MediaCodec::Java_MediaCodec_flush(env, j_media_codec_.obj());
}

void MediaCodecBridge::Stop() {
  JNIEnv* env = AttachCurrentThread();
  JNI_MediaCodec::Java_MediaCodec_stop(env, j_media_codec_.obj());
}

void MediaCodecBridge::GetOutputFormat(int* width, int* height) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_format(
     JNI_MediaCodec::Java_MediaCodec_getOutputFormat(
         env, j_media_codec_.obj()));
  if (!j_format.is_null()) {
    ScopedJavaLocalRef<jstring> j_key_width =
        ConvertUTF8ToJavaString(env, "width");
    *width = JNI_MediaFormat::Java_MediaFormat_getInteger(
        env, j_format.obj(), j_key_width.obj());

    ScopedJavaLocalRef<jstring> j_key_height =
        ConvertUTF8ToJavaString(env, "height");
    *height = JNI_MediaFormat::Java_MediaFormat_getInteger(
        env, j_format.obj(), j_key_height.obj());
  }
}

size_t MediaCodecBridge::QueueInputBuffer(
    int index, const uint8* data, int size,
    const base::TimeDelta& presentation_time) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_buffer(
      env, env->GetObjectArrayElement(j_input_buffers_.obj(), index));

  uint8* direct_buffer =
      static_cast<uint8*>(env->GetDirectBufferAddress(j_buffer.obj()));
  int64 buffer_capacity = env->GetDirectBufferCapacity(j_buffer.obj());

  size_t size_to_copy = (buffer_capacity < size) ? buffer_capacity : size;
  if (size_to_copy > 0)
    memcpy(direct_buffer, data, size_to_copy);

  JNI_MediaCodec::Java_MediaCodec_queueInputBuffer(
      env, j_media_codec_.obj(),
      index, 0, size_to_copy, presentation_time.InMicroseconds(), 0);
  return size_to_copy;
}

void MediaCodecBridge::QueueEOS(int input_buffer_index) {
  JNIEnv* env = AttachCurrentThread();
  JNI_MediaCodec::Java_MediaCodec_queueInputBuffer(
      env, j_media_codec_.obj(),
      input_buffer_index, 0, 0, 0, kBufferFlagEndOfStream);
}

int MediaCodecBridge::DequeueInputBuffer(base::TimeDelta timeout) {
  JNIEnv* env = AttachCurrentThread();
  return JNI_MediaCodec::Java_MediaCodec_dequeueInputBuffer(
      env, j_media_codec_.obj(), timeout.InMicroseconds());
}

int MediaCodecBridge::DequeueOutputBuffer(
    base::TimeDelta timeout, int* offset, int* size,
    base::TimeDelta* presentation_time, bool* end_of_stream) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_info(
      Java_MediaCodecBufferInfo_Constructor(env));
  jint j_buffer = JNI_MediaCodec::Java_MediaCodec_dequeueOutputBuffer(
      env, j_media_codec_.obj(), j_info.obj(), timeout.InMicroseconds());

  if (j_buffer >= 0) {
    int64 presentation_time_us;
    int flags;
    GetBufferInfo(
        env, j_info.obj(), offset, size, &presentation_time_us, &flags);
    *presentation_time =
        base::TimeDelta::FromMicroseconds(presentation_time_us);
    *end_of_stream = flags & kBufferFlagEndOfStream;
  }
  return j_buffer;
}

void MediaCodecBridge::ReleaseOutputBuffer(int index, bool render) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  JNI_MediaCodec::Java_MediaCodec_releaseOutputBuffer(
      env, j_media_codec_.obj(), index, render);
}

int MediaCodecBridge::GetInputBuffers() {
  JNIEnv* env = AttachCurrentThread();

  j_input_buffers_.Reset(
      JNI_MediaCodec::Java_MediaCodec_getInputBuffers(
          env, j_media_codec_.obj()));

  return env->GetArrayLength(j_input_buffers_.obj());
}

int MediaCodecBridge::GetOutputBuffers() {
  JNIEnv* env = AttachCurrentThread();

  j_output_buffers_.Reset(
      JNI_MediaCodec::Java_MediaCodec_getOutputBuffers(
          env, j_media_codec_.obj()));

  return env->GetArrayLength(j_output_buffers_.obj());
}

}  // namespace media

