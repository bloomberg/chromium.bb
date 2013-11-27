// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/coremedia_glue.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/lazy_instance.h"

namespace {

// This class is used to retrieve some CoreMedia library functions. It must be
// used as a LazyInstance so that it is initialised once and in a thread-safe
// way. Normally no work is done in constructors: LazyInstance is an exception.
class CoreMediaLibraryInternal {
 public:
  typedef CoreMediaGlue::CMTime (*CMTimeMakeMethod)(int64_t, int32_t);
  typedef CVImageBufferRef (*CMSampleBufferGetImageBufferMethod)(
      CoreMediaGlue::CMSampleBufferRef);

  CoreMediaLibraryInternal() {
    NSBundle* bundle = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/CoreMedia.framework"];

    const char* path = [[bundle executablePath] fileSystemRepresentation];
    CHECK(path);
    void* library_handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    CHECK(library_handle) << dlerror();

    // Now extract the methods.
    cm_time_make_ = reinterpret_cast<CMTimeMakeMethod>(
        dlsym(library_handle, "CMTimeMake"));
    CHECK(cm_time_make_) << dlerror();

    cm_sample_buffer_get_image_buffer_method_ =
        reinterpret_cast<CMSampleBufferGetImageBufferMethod>(
            dlsym(library_handle, "CMSampleBufferGetImageBuffer"));
    CHECK(cm_sample_buffer_get_image_buffer_method_) << dlerror();
  }

  const CMTimeMakeMethod& cm_time_make() const { return cm_time_make_; }
  const CMSampleBufferGetImageBufferMethod&
      cm_sample_buffer_get_image_buffer_method() const {
    return cm_sample_buffer_get_image_buffer_method_;
  }

 private:
  CMTimeMakeMethod cm_time_make_;
  CMSampleBufferGetImageBufferMethod cm_sample_buffer_get_image_buffer_method_;

  DISALLOW_COPY_AND_ASSIGN(CoreMediaLibraryInternal);
};

}  // namespace

static base::LazyInstance<CoreMediaLibraryInternal> g_coremedia_handle =
    LAZY_INSTANCE_INITIALIZER;

CoreMediaGlue::CMTime CoreMediaGlue::CMTimeMake(int64_t value,
                                                int32_t timescale) {
  return g_coremedia_handle.Get().cm_time_make()(value, timescale);
}

CVImageBufferRef CoreMediaGlue::CMSampleBufferGetImageBuffer(
    CMSampleBufferRef buffer) {
  return g_coremedia_handle.Get().cm_sample_buffer_get_image_buffer_method()(
      buffer);
}
