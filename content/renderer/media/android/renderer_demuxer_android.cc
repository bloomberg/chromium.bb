// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_demuxer_android.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/renderer/media/android/media_source_delegate.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/webmediaplayer_android.h"

namespace content {

RendererDemuxerAndroid::RendererDemuxerAndroid(RenderView* render_view)
    : RenderViewObserver(render_view) {}

RendererDemuxerAndroid::~RendererDemuxerAndroid() {}

void RendererDemuxerAndroid::AddDelegate(int demuxer_client_id,
                                         MediaSourceDelegate* delegate) {
  delegates_.AddWithID(delegate, demuxer_client_id);
}

void RendererDemuxerAndroid::RemoveDelegate(int demuxer_client_id) {
  delegates_.Remove(demuxer_client_id);
}

bool RendererDemuxerAndroid::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererDemuxerAndroid, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_ReadFromDemuxer, OnReadFromDemuxer)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaSeekRequest, OnMediaSeekRequest)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaConfigRequest, OnMediaConfigRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererDemuxerAndroid::DemuxerReady(
    int demuxer_client_id,
    const media::DemuxerConfigs& configs) {
  Send(new MediaPlayerHostMsg_DemuxerReady(
      routing_id(), demuxer_client_id, configs));
}

void RendererDemuxerAndroid::ReadFromDemuxerAck(
    int demuxer_client_id,
    const media::DemuxerData& data) {
  Send(new MediaPlayerHostMsg_ReadFromDemuxerAck(
      routing_id(), demuxer_client_id, data));
}

void RendererDemuxerAndroid::SeekRequestAck(int demuxer_client_id,
                                            unsigned seek_request_id) {
  Send(new MediaPlayerHostMsg_MediaSeekRequestAck(
      routing_id(), demuxer_client_id, seek_request_id));
}

void RendererDemuxerAndroid::DurationChanged(int demuxer_client_id,
                                             const base::TimeDelta& duration) {
  Send(new MediaPlayerHostMsg_DurationChanged(
      routing_id(), demuxer_client_id, duration));
}

void RendererDemuxerAndroid::OnReadFromDemuxer(
    int demuxer_client_id,
    media::DemuxerStream::Type type) {
  MediaSourceDelegate* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->OnReadFromDemuxer(type);
}

void RendererDemuxerAndroid::OnMediaSeekRequest(
    int demuxer_client_id,
    const base::TimeDelta& time_to_seek,
    unsigned seek_request_id) {
  MediaSourceDelegate* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->Seek(time_to_seek, seek_request_id);
}

void RendererDemuxerAndroid::OnMediaConfigRequest(int demuxer_client_id) {
  MediaSourceDelegate* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->OnMediaConfigRequest();
}

}  // namespace content
