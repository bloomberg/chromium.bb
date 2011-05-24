// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media_stream/media_stream_provider.h"

namespace media_stream {

MediaCaptureDeviceInfo::MediaCaptureDeviceInfo()
    : stream_type(kNoService),
      name(),
      device_id(),
      in_use(false) {}

MediaCaptureDeviceInfo::MediaCaptureDeviceInfo(MediaStreamType service_param,
                                               const std::string name_param,
                                               const std::string device_param,
                                               bool opened)
    : stream_type(service_param),
      name(name_param),
      device_id(device_param),
      in_use(opened) {}

MediaStreamProviderListener::~MediaStreamProviderListener() {}

MediaStreamProvider::~MediaStreamProvider() {}

}  // namespace

