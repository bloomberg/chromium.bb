// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/webaudio_media_codec_bridge.h"

#include <errno.h>
#include <unistd.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "jni/WebAudioMediaCodecBridge_jni.h"
#include "media/base/android/webaudio_media_codec_info.h"


using base::android::AttachCurrentThread;

namespace media {

void WebAudioMediaCodecBridge::RunWebAudioMediaCodec(
    base::SharedMemoryHandle encoded_audio_handle,
    base::FileDescriptor pcm_output,
    size_t data_size) {
  WebAudioMediaCodecBridge bridge(encoded_audio_handle, pcm_output, data_size);

  bridge.DecodeInMemoryAudioFile();
}

WebAudioMediaCodecBridge::WebAudioMediaCodecBridge(
    base::SharedMemoryHandle encoded_audio_handle,
    base::FileDescriptor pcm_output,
    size_t data_size)
    : encoded_audio_handle_(encoded_audio_handle.fd),
      pcm_output_(pcm_output.fd),
      data_size_(data_size) {
  DVLOG(1) << "WebAudioMediaCodecBridge start **********************"
           << "input fd = " << encoded_audio_handle_
           << " output fd = " << pcm_output.fd;
}

WebAudioMediaCodecBridge::~WebAudioMediaCodecBridge() {
  if (close(pcm_output_)) {
    DVLOG(1) << "Couldn't close output fd " << pcm_output_
             << ": " << strerror(errno);
  }

  if (close(encoded_audio_handle_)) {
    DVLOG(1) << "Couldn't close shared mem fd " << encoded_audio_handle_
             << ": " << strerror(errno);
  }
}

bool WebAudioMediaCodecBridge::DecodeInMemoryAudioFile() {
  // Process the encoded data from |encoded_data_handle_|.

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  jboolean decoded = Java_WebAudioMediaCodecBridge_decodeAudioFile(
      env,
      base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(this),
      encoded_audio_handle_,
      data_size_);

  DVLOG(1) << "decoded = " << decoded;
  return decoded;
}

void WebAudioMediaCodecBridge::InitializeDestination(
    JNIEnv* env,
    jobject /*java object*/,
    jint channel_count,
    jint sample_rate,
    jlong duration_microsec) {
  // Send information about this audio file: number of channels,
  // sample rate (Hz), and the number of frames.
  struct WebAudioMediaCodecInfo info = {
    static_cast<unsigned long>(channel_count),
    static_cast<unsigned long>(sample_rate),
    // The number of frames is the duration of the file
    // (in microseconds) times the sample rate.
    static_cast<unsigned long>(
        0.5 + (duration_microsec * 0.000001 *
               sample_rate))
  };

  DVLOG(1) << "InitializeDestination:"
           << "  channel count = " << channel_count
           << "  rate = " << sample_rate
           << "  duration = " << duration_microsec << " microsec";

  HANDLE_EINTR(write(pcm_output_, &info, sizeof(info)));
}

void WebAudioMediaCodecBridge::OnChunkDecoded(
    JNIEnv* env,
    jobject /*java object*/,
    jobject buf,
    jint buf_size) {

  if (buf_size <= 0 || !buf)
    return;

  int8_t* buffer =
      static_cast<int8_t*>(env->GetDirectBufferAddress(buf));

  size_t count = static_cast<size_t>(buf_size);

  // Write out the data to the pipe in small chunks if necessary.
  while (count > 0) {
    int bytes_to_write = (count >= PIPE_BUF) ? PIPE_BUF : count;
    ssize_t bytes_written = HANDLE_EINTR(write(pcm_output_,
                                               buffer,
                                               bytes_to_write));
    if (bytes_written == -1)
      break;
    count -= bytes_written;
    buffer += bytes_written;
  }
}

bool WebAudioMediaCodecBridge::RegisterWebAudioMediaCodecBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace
