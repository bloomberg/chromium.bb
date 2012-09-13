// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"

class CONTENT_EXPORT MediaStreamSourceExtraData
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamSource::ExtraData) {
 public:
  explicit MediaStreamSourceExtraData(
      const media_stream::StreamDeviceInfo& device_info)
      : device_info_(device_info) {
  }

  // Return device information about the camera or microphone.
  const media_stream::StreamDeviceInfo& device_info() const {
    return device_info_;
  }

 private:
  media_stream::StreamDeviceInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamSourceExtraData);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_
