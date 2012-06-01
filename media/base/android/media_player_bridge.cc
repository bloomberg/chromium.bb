// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"

// Auto generated jni class from MediaPlayerListener.java.
// Check base/android/jni_generator/golden_sample_for_tests_jni.h for example.
#include "jni/media_player_listener_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// These constants are from the android source tree and need to be kept in
// sync with android/media/MediaMetadata.java.
static const jint kPauseAvailable = 1;
static const jint kSeekBackwardAvailable = 2;
static const jint kSeekForwardAvailable = 3;

// This needs to be kept in sync with android.os.PowerManager
static const int kAndroidFullWakeLock = 26;

namespace media {

MediaPlayerBridge::MediaPlayerBridge() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  j_media_player_class_.Reset(GetClass(env, "android/media/MediaPlayer"));

  jmethodID constructor = GetMethodID(env,
                                      j_media_player_class_,
                                      "<init>",
                                      "()V");
  ScopedJavaLocalRef<jobject> tmp(env,
      env->NewObject(j_media_player_class_.obj(), constructor));
  j_media_player_.Reset(tmp);

  ScopedJavaLocalRef<jobject> j_listener(
      Java_MediaPlayerListener_create(env,
                                      reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_listener.is_null());

  // Set it as the various listeners.
  const char* listeners[] = {
      "OnBufferingUpdateListener",
      "OnCompletionListener",
      "OnErrorListener",
      "OnInfoListener",
      "OnPreparedListener",
      "OnSeekCompleteListener",
      "OnVideoSizeChangedListener",
  };
  for (unsigned int i = 0; i < arraysize(listeners); ++i) {
    std::string signature = StringPrintf("(Landroid/media/MediaPlayer$%s;)V",
                                         listeners[i]);
    std::string method_name = StringPrintf("set%s", listeners[i]);
    jmethodID method = GetMethodID(env,
                                   j_media_player_class_,
                                   method_name.c_str(),
                                   signature.c_str());
    env->CallVoidMethod(j_media_player_.obj(), method, j_listener.obj());
    CheckException(env);
  }
}

MediaPlayerBridge::~MediaPlayerBridge() {
  SetVideoSurface(NULL);
  CallVoidMethod("release");
}

void MediaPlayerBridge::SetDataSource(
    const std::string& url,
    const std::string& cookies,
    bool hide_url_log) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  // Create a Java String for the URL.
  ScopedJavaLocalRef<jstring> j_url_string = ConvertUTF8ToJavaString(env, url);

  // Create the android.net.Uri object.
  ScopedJavaLocalRef<jclass> cls(GetClass(env, "android/net/Uri"));
  jmethodID method = GetStaticMethodID(env, cls,
      "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
  ScopedJavaLocalRef<jobject> j_uri(env,
      env->CallStaticObjectMethod(cls.obj(), method, j_url_string.obj()));

  // Create the java.util.Map.
  cls.Reset(GetClass(env, "java/util/HashMap"));
  jmethodID constructor = GetMethodID(env, cls, "<init>", "()V");
  ScopedJavaLocalRef<jobject> j_map(env,
      env->NewObject(cls.obj(), constructor));
  jmethodID put_method = GetMethodID(env, cls, "put",
      "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  // Construct headers that needs to be sent with the url.
  HeadersMap headers;
  // For incognito mode, we need a header to hide url log.
  if (hide_url_log)
    headers.insert(std::make_pair("x-hide-urls-from-log", "true"));
  // If cookies are present, add them in the header.
  if (!cookies.empty())
    headers.insert(std::make_pair("Cookie", cookies));

  // Fill the Map with the headers.
  for (HeadersMap::const_iterator iter = headers.begin();
       iter != headers.end(); ++iter) {
    ScopedJavaLocalRef<jstring> key = ConvertUTF8ToJavaString(env, iter->first);
    ScopedJavaLocalRef<jstring> value =
        ConvertUTF8ToJavaString(env, iter->second);
    ScopedJavaLocalRef<jobject> result(env,
        env->CallObjectMethod(j_map.obj(), put_method, key.obj(), value.obj()));
  }

  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  // Finally- Call the setDataSource method.
  jmethodID set_data_source =
      GetMethodID(env, j_media_player_class_, "setDataSource",
      "(Landroid/content/Context;Landroid/net/Uri;Ljava/util/Map;)V");
  DCHECK(set_data_source);
  env->CallVoidMethod(j_media_player_.obj(), set_data_source, j_context,
                      j_uri.obj(), j_map.obj());
  CheckException(env);
}

void MediaPlayerBridge::SetVideoSurface(jobject surface) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "setSurface",
                                 "(Landroid/view/Surface;)V");
  env->CallVoidMethod(j_media_player_.obj(), method, surface);
  CheckException(env);
}

void MediaPlayerBridge::Prepare(const MediaInfoCB& media_info_cb,
                                const MediaErrorCB& media_error_cb,
                                const VideoSizeChangedCB& video_size_changed_cb,
                                const BufferingUpdateCB& buffering_update_cb,
                                const base::Closure& media_prepared_cb) {
  media_info_cb_ = media_info_cb;
  media_error_cb_ = media_error_cb,
  video_size_changed_cb_ = video_size_changed_cb;
  buffering_update_cb_ = buffering_update_cb;
  media_prepared_cb_ = media_prepared_cb;
  CallVoidMethod("prepareAsync");
}

void MediaPlayerBridge::Start(const base::Closure& playback_complete_cb) {
  playback_complete_cb_ = playback_complete_cb;
  CallVoidMethod("start");
}

void MediaPlayerBridge::Pause() {
  CallVoidMethod("pause");
}

void MediaPlayerBridge::Stop() {
  CallVoidMethod("stop");
}

bool MediaPlayerBridge::IsPlaying() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "isPlaying",
                                 "()Z");
  jint result = env->CallBooleanMethod(j_media_player_.obj(), method);
  CheckException(env);

  return result;
}

int MediaPlayerBridge::GetVideoWidth() {
  return CallIntMethod("getVideoWidth");
}

int MediaPlayerBridge::GetVideoHeight() {
  return CallIntMethod("getVideoHeight");
}

void MediaPlayerBridge::SeekTo(base::TimeDelta time,
                               const base::Closure& seek_complete_cb) {
  seek_complete_cb_ = seek_complete_cb;
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env, j_media_player_class_, "seekTo", "(I)V");
  DCHECK(method);
  int time_msec = static_cast<int>(time.InMilliseconds());
  DCHECK_EQ(time.InMilliseconds(), static_cast<int64>(time_msec));
  env->CallVoidMethod(j_media_player_.obj(),
                      method,
                      time_msec);
  CheckException(env);
}

base::TimeDelta MediaPlayerBridge::GetCurrentTime() {
  return base::TimeDelta::FromMilliseconds(CallIntMethod("getCurrentPosition"));
}

base::TimeDelta MediaPlayerBridge::GetDuration() {
  return base::TimeDelta::FromMilliseconds(CallIntMethod("getDuration"));
}

void MediaPlayerBridge::Reset() {
  CallVoidMethod("reset");
}

void MediaPlayerBridge::SetVolume(float left_volume, float right_volume) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "setVolume",
                                 "(FF)V");
  DCHECK(method);
  env->CallVoidMethod(j_media_player_.obj(), method,
                      left_volume, right_volume);
  CheckException(env);
}

void MediaPlayerBridge::SetStayAwakeWhilePlaying() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);
  jint j_mode = kAndroidFullWakeLock;
  jmethodID method = GetMethodID(env, j_media_player_class_,
      "setWakeMode", "(Landroid/content/Context;I)V");
  env->CallVoidMethod(j_media_player_.obj(), method, j_context, j_mode);
  CheckException(env);
}

void MediaPlayerBridge::OnMediaError(JNIEnv* /* env */,
                                     jobject /* obj */,
                                     jint error_type) {
  media_error_cb_.Run(error_type);
}

void MediaPlayerBridge::OnMediaInfo(JNIEnv* /* env */,
                                    jobject /* obj */,
                                    jint info_type) {
  media_info_cb_.Run(info_type);
}

void MediaPlayerBridge::OnVideoSizeChanged(JNIEnv* /* env */,
                                           jobject /* obj */,
                                           jint width,
                                           jint height) {
  video_size_changed_cb_.Run(width, height);
}

void MediaPlayerBridge::OnBufferingUpdate(JNIEnv* /* env */,
                                          jobject /* obj */,
                                          jint percent) {
  buffering_update_cb_.Run(percent);
}

void MediaPlayerBridge::OnPlaybackComplete(JNIEnv* /* env */,
                                           jobject /* obj */) {
  playback_complete_cb_.Run();
}

void MediaPlayerBridge::OnSeekComplete(JNIEnv* /* env */,
                                       jobject /* obj */) {
  seek_complete_cb_.Run();
}

void MediaPlayerBridge::OnMediaPrepared(JNIEnv* /* env */,
                                        jobject /* obj */) {
  media_prepared_cb_.Run();
}

void MediaPlayerBridge::GetMetadata(bool* can_pause,
                                    bool* can_seek_forward,
                                    bool* can_seek_backward) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "getMetadata",
                                 "(ZZ)Landroid/media/Metadata;");
  ScopedJavaLocalRef<jobject> j_metadata(env,
      env->CallObjectMethod(j_media_player_.obj(),
      method, JNI_FALSE, JNI_FALSE));
  CheckException(env);
  if (j_metadata.is_null())
    return;

  ScopedJavaLocalRef<jclass> cls(GetClass(env, "android/media/Metadata"));
  jmethodID get_boolean = GetMethodID(env, cls, "getBoolean", "(I)Z");
  *can_pause = env->CallBooleanMethod(j_metadata.obj(),
                                      get_boolean,
                                      kPauseAvailable);
  CheckException(env);
  *can_seek_backward = env->CallBooleanMethod(j_metadata.obj(),
                                              get_boolean,
                                              kSeekBackwardAvailable);
  CheckException(env);
  *can_seek_forward = env->CallBooleanMethod(j_metadata.obj(),
                                             get_boolean,
                                             kSeekForwardAvailable);
  CheckException(env);
}

// ---- JNI Helpers for repeated call patterns. ----

void MediaPlayerBridge::CallVoidMethod(std::string method_name) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 method_name.c_str(),
                                 "()V");
  env->CallVoidMethod(j_media_player_.obj(), method);
  CheckException(env);
}

int MediaPlayerBridge::CallIntMethod(std::string method_name) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 method_name.c_str(),
                                 "()I");
  jint j_result = env->CallIntMethod(j_media_player_.obj(), method);
  CheckException(env);
  return j_result;
}

bool MediaPlayerBridge::RegisterMediaPlayerListener(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_MediaPlayerListener_clazz);
  return ret;
}

}  // namespace media
