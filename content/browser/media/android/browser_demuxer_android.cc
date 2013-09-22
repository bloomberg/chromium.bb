// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_demuxer_android.h"

#include "content/common/media/media_player_messages_android.h"

namespace content {

BrowserDemuxerAndroid::BrowserDemuxerAndroid() {}

BrowserDemuxerAndroid::~BrowserDemuxerAndroid() {}

void BrowserDemuxerAndroid::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case MediaPlayerHostMsg_DemuxerReady::ID:
    case MediaPlayerHostMsg_ReadFromDemuxerAck::ID:
    case MediaPlayerHostMsg_DurationChanged::ID:
    case MediaPlayerHostMsg_MediaSeekRequestAck::ID:
      *thread = BrowserThread::UI;
      return;
  }
}

bool BrowserDemuxerAndroid::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(BrowserDemuxerAndroid, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DemuxerReady, OnDemuxerReady)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ReadFromDemuxerAck,
                        OnReadFromDemuxerAck)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DurationChanged,
                        OnDurationChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaSeekRequestAck,
                        OnMediaSeekRequestAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserDemuxerAndroid::AddDemuxerClient(
    int demuxer_client_id,
    media::DemuxerAndroidClient* client) {
  DVLOG(1) << __FUNCTION__ << " peer_pid=" << peer_pid()
           << " demuxer_client_id=" << demuxer_client_id;
  demuxer_clients_.AddWithID(client, demuxer_client_id);
}

void BrowserDemuxerAndroid::RemoveDemuxerClient(int demuxer_client_id) {
  DVLOG(1) << __FUNCTION__ << " peer_pid=" << peer_pid()
           << " demuxer_client_id=" << demuxer_client_id;
  demuxer_clients_.Remove(demuxer_client_id);
}

void BrowserDemuxerAndroid::RequestDemuxerData(
    int demuxer_client_id, media::DemuxerStream::Type type) {
  DCHECK(demuxer_clients_.Lookup(demuxer_client_id)) << demuxer_client_id;
  Send(new MediaPlayerMsg_ReadFromDemuxer(demuxer_client_id, type));
}

void BrowserDemuxerAndroid::RequestDemuxerSeek(int demuxer_client_id,
                                               base::TimeDelta time_to_seek,
                                               unsigned seek_request_id) {
  DCHECK(demuxer_clients_.Lookup(demuxer_client_id)) << demuxer_client_id;
  Send(new MediaPlayerMsg_MediaSeekRequest(
      demuxer_client_id, time_to_seek, seek_request_id));
}

void BrowserDemuxerAndroid::RequestDemuxerConfigs(int demuxer_client_id) {
  DCHECK(demuxer_clients_.Lookup(demuxer_client_id)) << demuxer_client_id;
  Send(new MediaPlayerMsg_MediaConfigRequest(demuxer_client_id));
}

void BrowserDemuxerAndroid::OnDemuxerReady(
    int demuxer_client_id,
    const media::DemuxerConfigs& configs) {
  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);
  if (client)
    client->OnDemuxerConfigsAvailable(configs);
}

void BrowserDemuxerAndroid::OnReadFromDemuxerAck(
    int demuxer_client_id,
    const media::DemuxerData& data) {
  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);
  if (client)
    client->OnDemuxerDataAvailable(data);
}

void BrowserDemuxerAndroid::OnMediaSeekRequestAck(int demuxer_client_id,
                                                  unsigned seek_request_id) {
  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);
  if (client)
    client->OnDemuxerSeeked(seek_request_id);
}

void BrowserDemuxerAndroid::OnDurationChanged(int demuxer_client_id,
                                              const base::TimeDelta& duration) {
  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);
  if (client)
    client->OnDemuxerDurationChanged(duration);
}

}  // namespace content
