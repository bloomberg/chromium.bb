// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_DEMUXER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_DEMUXER_ANDROID_H_

#include "base/id_map.h"
#include "content/public/renderer/render_view_observer.h"
#include "media/base/android/demuxer_stream_player_params.h"

namespace content {

class MediaSourceDelegate;

// Represents the renderer process half of an IPC-based implementation of
// media::DemuxerAndroid.
//
// Refer to BrowserDemuxerAndroid for the browser process half.
class RendererDemuxerAndroid : public RenderViewObserver {
 public:
  explicit RendererDemuxerAndroid(RenderView* render_view);
  virtual ~RendererDemuxerAndroid();

  // Associates |delegate| with |demuxer_client_id| for handling incoming IPC
  // messages.
  void AddDelegate(int demuxer_client_id, MediaSourceDelegate* delegate);

  // Removes the association created by AddDelegate().
  void RemoveDelegate(int demuxer_client_id);

  // RenderViewObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // media::DemuxerAndroidClient "implementation".
  //
  // TODO(scherkus): Consider having RendererDemuxerAndroid actually implement
  // media::DemuxerAndroidClient and MediaSourceDelegate actually implement
  // media::DemuxerAndroid.
  void DemuxerReady(int demuxer_client_id,
                    const media::DemuxerConfigs& configs);
  void ReadFromDemuxerAck(int demuxer_client_id,
                          const media::DemuxerData& data);
  void SeekRequestAck(int demuxer_client_id, unsigned seek_request_id);
  void DurationChanged(int demuxer_client_id, const base::TimeDelta& duration);

 private:
  void OnReadFromDemuxer(int demuxer_client_id,
                         media::DemuxerStream::Type type);
  void OnMediaSeekRequest(int demuxer_client_id,
                          const base::TimeDelta& time_to_seek,
                          unsigned seek_request_id);
  void OnMediaConfigRequest(int demuxer_client_id);

  IDMap<MediaSourceDelegate> delegates_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RendererDemuxerAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_DEMUXER_ANDROID_H_
