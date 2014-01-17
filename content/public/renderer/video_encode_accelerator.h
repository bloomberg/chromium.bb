// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_VIDEO_ENCODE_ACCELERATOR_H_
#define CONTENT_PUBLIC_RENDERER_VIDEO_ENCODE_ACCELERATOR_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

// Generate an instance of media::VideoEncodeAccelerator.
CONTENT_EXPORT scoped_ptr<media::VideoEncodeAccelerator>
CreateVideoEncodeAccelerator(media::VideoEncodeAccelerator::Client* client);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_VIDEO_ENCODE_ACCELERATOR_H_
