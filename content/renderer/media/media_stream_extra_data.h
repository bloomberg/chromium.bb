// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_EXTRA_DATA_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"

namespace webrtc {
class MediaStreamInterface;
}  // namespace webrtc

namespace content {

class CONTENT_EXPORT MediaStreamExtraData
    : NON_EXPORTED_BASE(public WebKit::WebMediaStream::ExtraData) {
 public:
  typedef base::Callback<void(const std::string& label)> StreamStopCallback;

  MediaStreamExtraData(webrtc::MediaStreamInterface* stream, bool is_local);
  virtual ~MediaStreamExtraData();

  bool is_local() const { return is_local_; }

  void SetLocalStreamStopCallback(
      const StreamStopCallback& stop_callback);
  void OnLocalStreamStop();

  const scoped_refptr<webrtc::MediaStreamInterface>& stream() const {
    return stream_;
  }
 private:
  StreamStopCallback stream_stop_callback_;
  scoped_refptr<webrtc::MediaStreamInterface> stream_;
  bool is_local_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamExtraData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_EXTRA_DATA_H_
