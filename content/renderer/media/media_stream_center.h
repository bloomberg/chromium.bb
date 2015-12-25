// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"

namespace blink {
class WebAudioSourceProvider;
class WebMediaStreamCenterClient;
}

namespace content {
class PeerConnectionDependencyFactory;

class CONTENT_EXPORT MediaStreamCenter
    : NON_EXPORTED_BASE(public blink::WebMediaStreamCenter) {
 public:
  MediaStreamCenter(blink::WebMediaStreamCenterClient* client,
                    PeerConnectionDependencyFactory* factory);
  ~MediaStreamCenter() override;

 private:
  void didCreateMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void didEnableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void didDisableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void didStopLocalMediaStream(const blink::WebMediaStream& stream) override;

  bool didStopMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  blink::WebAudioSourceProvider* createWebAudioSourceFromMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void didCreateMediaStream(blink::WebMediaStream& stream) override;

  bool didAddMediaStreamTrack(const blink::WebMediaStream& stream,
                              const blink::WebMediaStreamTrack& track) override;

  bool didRemoveMediaStreamTrack(
      const blink::WebMediaStream& stream,
      const blink::WebMediaStreamTrack& track) override;

  // |rtc_factory_| is a weak pointer and is owned by the RenderThreadImpl.
  // It is valid as long as  RenderThreadImpl exist.
  PeerConnectionDependencyFactory* rtc_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamCenter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
