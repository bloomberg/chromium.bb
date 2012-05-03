// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_IMPL_H_

#include <map>
#include <string>

#include "content/renderer/media/media_stream_impl.h"

class MockMediaStreamImpl : public MediaStreamImpl {
 public:
  MockMediaStreamImpl();
  virtual ~MockMediaStreamImpl();

  virtual WebKit::WebPeerConnectionHandler* CreatePeerConnectionHandler(
      WebKit::WebPeerConnectionHandlerClient* client) OVERRIDE;
  virtual void ClosePeerConnection(
      PeerConnectionHandlerBase* pc_handler) OVERRIDE;

  virtual webrtc::LocalMediaStreamInterface* GetLocalMediaStream(
        const WebKit::WebMediaStreamDescriptor& stream) OVERRIDE;

  // Implement webkit_glue::MediaStreamClient.
  virtual scoped_refptr<media::VideoDecoder> GetVideoDecoder(
      const GURL& url,
      media::MessageLoopFactory* message_loop_factory) OVERRIDE;

  // Implement MediaStreamDispatcherEventHandler.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& audio_array,
      const media_stream::StreamDeviceInfoArray& video_array) OVERRIDE;
  virtual void OnStreamGenerationFailed(int request_id) OVERRIDE;
  virtual void OnVideoDeviceFailed(
      const std::string& label,
      int index) OVERRIDE;
  virtual void OnAudioDeviceFailed(
      const std::string& label,
      int index) OVERRIDE;

  void AddLocalStream(webrtc::LocalMediaStreamInterface* stream);
 private:
  typedef std::map<std::string,
      talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> >
      MockMediaStreamPtrMap;
  MockMediaStreamPtrMap mock_local_streams_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_IMPL_H_
