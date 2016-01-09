// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"

#include <stddef.h>

#include <algorithm>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "jni/MediaCodecUtil_jni.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
const std::string CodecTypeToAndroidMimeType(const std::string& codec) {
  // TODO(xhwang): Shall we handle more detailed strings like "mp4a.40.2"?
  if (codec == "avc1")
    return "video/avc";
  if (codec == "hvc1")
    return "video/hevc";
  if (codec == "mp4a")
    return "audio/mp4a-latm";
  if (codec == "vp8" || codec == "vp8.0")
    return "video/x-vnd.on2.vp8";
  if (codec == "vp9" || codec == "vp9.0")
    return "video/x-vnd.on2.vp9";
  if (codec == "vorbis")
    return "audio/vorbis";
  if (codec == "opus")
    return "audio/opus";
  return std::string();
}

// TODO(qinmin): using a map to help all the conversions in this class.
const std::string AndroidMimeTypeToCodecType(const std::string& mime) {
  if (mime == "video/mp4v-es")
    return "mp4v";
  if (mime == "video/avc")
    return "avc1";
  if (mime == "video/hevc")
    return "hvc1";
  if (mime == "video/x-vnd.on2.vp8")
    return "vp8";
  if (mime == "video/x-vnd.on2.vp9")
    return "vp9";
  if (mime == "audio/mp4a-latm")
    return "mp4a";
  if (mime == "audio/mpeg")
    return "mp3";
  if (mime == "audio/vorbis")
    return "vorbis";
  if (mime == "audio/opus")
    return "opus";
  return std::string();
}

std::string GetDefaultCodecName(const std::string& mime_type,
                                MediaCodecDirection direction) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return std::string();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> j_codec_name =
      Java_MediaCodecUtil_getDefaultCodecName(env, j_mime.obj(), direction);
  return ConvertJavaStringToUTF8(env, j_codec_name.obj());
}

bool SupportsGetName() {
  // MediaCodec.getName() is only available on JB MR2 and greater.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 18;
}

// Represents supported codecs on android.
// TODO(qinmin): Currently the codecs string only contains one codec. Do we
// need to support codecs separated by comma. (e.g. "vp8" -> "vp8, vp8.0")?
struct CodecsInfo {
  std::string codecs;  // E.g. "vp8" or "avc1".
  std::string name;    // E.g. "OMX.google.vp8.decoder".
  MediaCodecDirection direction;
};

// Get a list of supported codecs.
std::vector<CodecsInfo> GetCodecsInfo() {
  std::vector<CodecsInfo> codecs_info;
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return codecs_info;

  JNIEnv* env = AttachCurrentThread();
  std::string mime_type;
  ScopedJavaLocalRef<jobjectArray> j_codec_info_array =
      Java_MediaCodecUtil_getCodecsInfo(env);
  jsize len = env->GetArrayLength(j_codec_info_array.obj());
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_info(
        env, env->GetObjectArrayElement(j_codec_info_array.obj(), i));
    ScopedJavaLocalRef<jstring> j_codec_type =
        Java_CodecInfo_codecType(env, j_info.obj());
    ConvertJavaStringToUTF8(env, j_codec_type.obj(), &mime_type);
    ScopedJavaLocalRef<jstring> j_codec_name =
        Java_CodecInfo_codecName(env, j_info.obj());
    CodecsInfo info;
    info.codecs = AndroidMimeTypeToCodecType(mime_type);
    ConvertJavaStringToUTF8(env, j_codec_name.obj(), &info.name);
    info.direction = static_cast<MediaCodecDirection>(
        Java_CodecInfo_direction(env, j_info.obj()));
    codecs_info.push_back(info);
  }
  return codecs_info;
}

// static
bool MediaCodecUtil::IsMediaCodecAvailable() {
  // MediaCodec is only available on JB and greater.
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16)
    return false;
  // Blacklist some devices on Jellybean as for MediaCodec support is buggy.
  // http://crbug.com/365494.
  if (base::android::BuildInfo::GetInstance()->sdk_int() == 16) {
    std::string model(base::android::BuildInfo::GetInstance()->model());
    return model != "GT-I9100" && model != "GT-I9300" && model != "GT-N7000";
  }
  return true;
}

// static
bool MediaCodecUtil::SupportsSetParameters() {
  // MediaCodec.setParameters() is only available starting with K.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 19;
}

// static
std::set<int> MediaCodecUtil::GetEncoderColorFormats(
    const std::string& mime_type) {
  std::set<int> color_formats;
  if (!IsMediaCodecAvailable())
    return color_formats;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jintArray> j_color_format_array =
      Java_MediaCodecUtil_getEncoderColorFormatsForMime(env, j_mime.obj());

  if (j_color_format_array.obj()) {
    std::vector<int> formats;
    JavaIntArrayToIntVector(env, j_color_format_array.obj(), &formats);
    color_formats = std::set<int>(formats.begin(), formats.end());
  }

  return color_formats;
}

// static
bool MediaCodecUtil::CanDecode(const std::string& codec, bool is_secure) {
  if (!IsMediaCodecAvailable())
    return false;

  JNIEnv* env = AttachCurrentThread();
  std::string mime = CodecTypeToAndroidMimeType(codec);
  if (mime.empty())
    return false;
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  return Java_MediaCodecUtil_canDecode(env, j_mime.obj(), is_secure);
}

// static
bool MediaCodecUtil::IsKnownUnaccelerated(const std::string& mime_type,
                                          MediaCodecDirection direction) {
  if (!IsMediaCodecAvailable())
    return true;

  std::string codec_name;
  if (SupportsGetName()) {
    codec_name = GetDefaultCodecName(mime_type, direction);
  } else {
    std::string codec_type = AndroidMimeTypeToCodecType(mime_type);
    std::vector<CodecsInfo> codecs_info = GetCodecsInfo();
    for (size_t i = 0; i < codecs_info.size(); ++i) {
      if (codecs_info[i].codecs == codec_type &&
          codecs_info[i].direction == direction) {
        codec_name = codecs_info[i].name;
        break;
      }
    }
  }
  DVLOG(1) << __PRETTY_FUNCTION__ << "Default codec for " << mime_type << " : "
           << codec_name;
  // It would be nice if MediaCodecInfo externalized some notion of
  // HW-acceleration but it doesn't. Android Media guidance is that the
  // "OMX.google" prefix is always used for SW decoders, so that's what we
  // use. "OMX.SEC.*" codec is Samsung software implementation - report it
  // as unaccelerated as well. Also temporary blacklist Exynos and MediaTek
  // devices while HW decoder video freezes and distortions are
  // investigated - http://crbug.com/446974.
  if (codec_name.length() > 0) {
    return (base::StartsWith(codec_name, "OMX.google.",
                             base::CompareCase::SENSITIVE) ||
            base::StartsWith(codec_name, "OMX.SEC.",
                             base::CompareCase::SENSITIVE) ||
            base::StartsWith(codec_name, "OMX.MTK.",
                             base::CompareCase::SENSITIVE) ||
            base::StartsWith(codec_name, "OMX.Exynos.",
                             base::CompareCase::SENSITIVE));
  }
  return true;
}

// static
bool MediaCodecUtil::IsHLSPath(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile())
    return false;

  std::string path = url.path();
  return base::EndsWith(path, ".m3u8", base::CompareCase::INSENSITIVE_ASCII);
}

// static
bool MediaCodecUtil::IsHLSURL(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile())
    return false;

  std::string spec = url.spec();
  if (base::EndsWith(spec, ".m3u8", base::CompareCase::INSENSITIVE_ASCII))
    return true;

  return (spec.find("m3u8") != std::string::npos);
}

// static
bool MediaCodecUtil::RegisterMediaCodecUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace media
