// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/android_image_reader_compat.h"

#include <dlfcn.h>

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "media/base/media_switches.h"

#define LOAD_FUNCTION(lib, func)                            \
  do {                                                      \
    func##_ = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##_) {                                         \
      DLOG(ERROR) << "Unable to load function " << #func;   \
      return false;                                         \
    }                                                       \
  } while (0)

namespace media {

AndroidImageReader& AndroidImageReader::GetInstance() {
  // C++11 static local variable initialization is
  // thread-safe.
  static base::NoDestructor<AndroidImageReader> instance;
  return *instance;
}

bool AndroidImageReader::IsSupported() {
  return is_supported_;
}

AndroidImageReader::AndroidImageReader() {
  is_supported_ =
      base::FeatureList::IsEnabled(media::kAImageReaderVideoOutput) &&
      LoadFunctions();
}

bool AndroidImageReader::LoadFunctions() {
  // If the Chromium build requires __ANDROID_API__ >= 26 at some
  // point in the future, we could directly use the global functions instead of
  // dynamic loading. However, since this would be incompatible with pre-Oreo
  // devices, this is unlikely to happen in the foreseeable future, so we use
  // dynamic loading.

  // Functions are not present for android version older than OREO
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_OREO) {
    return false;
  }

  void* libmediandk = dlopen("libmediandk.so", RTLD_NOW);
  if (libmediandk == nullptr) {
    LOG(ERROR) << "Couldnt open libmediandk.so";
    return false;
  }

  LOAD_FUNCTION(libmediandk, AImage_delete);
  LOAD_FUNCTION(libmediandk, AImage_getHardwareBuffer);
  LOAD_FUNCTION(libmediandk, AImage_getWidth);
  LOAD_FUNCTION(libmediandk, AImage_getHeight);
  LOAD_FUNCTION(libmediandk, AImageReader_new);
  LOAD_FUNCTION(libmediandk, AImageReader_setImageListener);
  LOAD_FUNCTION(libmediandk, AImageReader_delete);
  LOAD_FUNCTION(libmediandk, AImageReader_getWindow);
  LOAD_FUNCTION(libmediandk, AImageReader_acquireLatestImageAsync);

  void* libandroid = dlopen("libandroid.so", RTLD_NOW);
  if (libandroid == nullptr) {
    LOG(ERROR) << "Couldnt open libandroid.so";
    return false;
  }

  LOAD_FUNCTION(libandroid, ANativeWindow_toSurface);

  return true;
}

void AndroidImageReader::AImage_delete(AImage* image) {
  AImage_delete_(image);
}

media_status_t AndroidImageReader::AImage_getHardwareBuffer(
    const AImage* image,
    AHardwareBuffer** buffer) {
  return AImage_getHardwareBuffer_(image, buffer);
}

media_status_t AndroidImageReader::AImage_getWidth(const AImage* image,
                                                   int32_t* width) {
  return AImage_getWidth_(image, width);
}

media_status_t AndroidImageReader::AImage_getHeight(const AImage* image,
                                                    int32_t* height) {
  return AImage_getHeight_(image, height);
}

media_status_t AndroidImageReader::AImageReader_new(int32_t width,
                                                    int32_t height,
                                                    int32_t format,
                                                    int32_t maxImages,
                                                    AImageReader** reader) {
  return AImageReader_new_(width, height, format, maxImages, reader);
}

media_status_t AndroidImageReader::AImageReader_setImageListener(
    AImageReader* reader,
    AImageReader_ImageListener* listener) {
  return AImageReader_setImageListener_(reader, listener);
}

void AndroidImageReader::AImageReader_delete(AImageReader* reader) {
  AImageReader_delete_(reader);
}

media_status_t AndroidImageReader::AImageReader_getWindow(
    AImageReader* reader,
    ANativeWindow** window) {
  return AImageReader_getWindow_(reader, window);
}

media_status_t AndroidImageReader::AImageReader_acquireLatestImageAsync(
    AImageReader* reader,
    AImage** image,
    int* acquireFenceFd) {
  return AImageReader_acquireLatestImageAsync_(reader, image, acquireFenceFd);
}

jobject AndroidImageReader::ANativeWindow_toSurface(JNIEnv* env,
                                                    ANativeWindow* window) {
  return ANativeWindow_toSurface_(env, window);
}

}  // namespace media
