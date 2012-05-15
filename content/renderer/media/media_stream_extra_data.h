// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/base/scoped_ref_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"

namespace webrtc {
class MediaStreamInterface;
class LocalMediaStreamInterface;
}  // namespace webrtc

class CONTENT_EXPORT MediaStreamExtraData
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamDescriptor::ExtraData) {
 public:
  explicit MediaStreamExtraData(webrtc::MediaStreamInterface* remote_stream);
  explicit MediaStreamExtraData(
      webrtc::LocalMediaStreamInterface* local_stream);
  virtual ~MediaStreamExtraData();

  webrtc::MediaStreamInterface* remote_stream() { return remote_stream_.get(); }
  webrtc::LocalMediaStreamInterface* local_stream() {
    return local_stream_.get();
  }

 private:
  talk_base::scoped_refptr<webrtc::MediaStreamInterface> remote_stream_;
  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> local_stream_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamExtraData);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_EXTRA_DATA_H_
