// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/media_keys.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/text_track.h"
#include "third_party/WebKit/public/web/WebMediaPlayer.h"

namespace media {
class ChunkDemuxer;
class DecoderBuffer;
class DemuxerStream;
class MediaLog;
struct MediaPlayerHostMsg_ReadFromDemuxerAck_Params;
}

namespace content {

class WebMediaPlayerProxyAndroid;

class MediaSourceDelegate : public media::DemuxerHost {
 public:
  typedef base::Callback<void(WebKit::WebMediaPlayer::NetworkState)>
      UpdateNetworkStateCB;
  typedef base::Callback<void(const base::TimeDelta&)> DurationChangeCB;

  // Helper class used by scoped_ptr to destroy an instance of
  // MediaSourceDelegate.
  class Destroyer {
   public:
    inline void operator()(void* media_source_delegate) const {
      static_cast<MediaSourceDelegate*>(media_source_delegate)->Destroy();
    }
  };

  MediaSourceDelegate(WebMediaPlayerProxyAndroid* proxy,
                      int player_id,
                      media::MediaLog* media_log);
  // Initialize the MediaSourceDelegate. |media_source| will be owned by
  // this object after this call.
  void InitializeMediaSource(
      WebKit::WebMediaSource* media_source,
      const media::NeedKeyCB& need_key_cb,
      const UpdateNetworkStateCB& update_network_state_cb,
      const DurationChangeCB& duration_change_cb);
#if defined(GOOGLE_TV)
  void InitializeMediaStream(
      media::Demuxer* demuxer,
      const UpdateNetworkStateCB& update_network_state_cb);
#endif

  const WebKit::WebTimeRanges& Buffered();
  size_t DecodedFrameCount() const;
  size_t DroppedFrameCount() const;
  size_t AudioDecodedByteCount() const;
  size_t VideoDecodedByteCount() const;

  void Seek(base::TimeDelta time);

  void CancelPendingSeek();

  void NotifyKeyAdded(const std::string& key_system);

  // Called when DemuxerStreamPlayer needs to read data from ChunkDemuxer.
  // If it's the first request after the seek, |seek_done| will be true.
  void OnReadFromDemuxer(media::DemuxerStream::Type type, bool seek_done);

  // Called when the player needs the new config data from ChunkDemuxer.
  void OnMediaConfigRequest();

  // Called by the Destroyer to destroy an instance of this object.
  void Destroy();

 private:
  // This is private to enforce use of the Destroyer.
  virtual ~MediaSourceDelegate();

  // Methods inherited from DemuxerHost.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void AddBufferedByteRange(int64 start, int64 end) OVERRIDE;
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE;
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void OnDemuxerError(media::PipelineStatus status) OVERRIDE;

  // Callbacks for ChunkDemuxer.
  void OnDemuxerInitDone(media::PipelineStatus status);
  void OnDemuxerStopDone();
  void OnDemuxerOpened();
  void OnNeedKey(const std::string& type,
                 const std::string& session_id,
                 scoped_ptr<uint8[]> init_data,
                 int init_data_size);
  scoped_ptr<media::TextTrack> OnAddTextTrack(media::TextKind kind,
                                              const std::string& label,
                                              const std::string& language);
  void NotifyDemuxerReady(const std::string& key_system);
  bool CanNotifyDemuxerReady();

  // Reads an access unit from the demuxer stream |stream| and stores it in
  // the |index|th access unit in |params|.
  void ReadFromDemuxerStream(
      media::DemuxerStream* stream,
      media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params,
      size_t index);
  void OnBufferReady(
      media::DemuxerStream* stream,
      media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params,
      size_t index,
      media::DemuxerStream::Status status,
      const scoped_refptr<media::DecoderBuffer>& buffer);

  // Helper function for calculating duration.
  int GetDurationMs();

  base::WeakPtrFactory<MediaSourceDelegate> weak_this_;

  WebMediaPlayerProxyAndroid* proxy_;
  int player_id_;

  scoped_refptr<media::MediaLog> media_log_;
  UpdateNetworkStateCB update_network_state_cb_;
  DurationChangeCB duration_change_cb_;

  scoped_ptr<media::ChunkDemuxer> chunk_demuxer_;
  scoped_ptr<WebKit::WebMediaSource> media_source_;
  media::Demuxer* demuxer_;

  media::PipelineStatistics statistics_;
  media::Ranges<base::TimeDelta> buffered_time_ranges_;
  // Keep a list of buffered time ranges.
  WebKit::WebTimeRanges buffered_web_time_ranges_;

  media::NeedKeyCB need_key_cb_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  WebKit::WebString current_key_system_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> audio_params_;
  scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> video_params_;

  bool seeking_;
  bool key_added_;
  size_t access_unit_size_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_
