// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
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
class DecryptingDemuxerStream;
class DemuxerStream;
class MediaLog;
struct MediaPlayerHostMsg_DemuxerReady_Params;
struct MediaPlayerHostMsg_ReadFromDemuxerAck_Params;
}

namespace content {

class WebMediaPlayerProxyAndroid;

class MediaSourceDelegate : public media::DemuxerHost {
 public:
  typedef base::Callback<void(WebKit::WebMediaSourceNew*)>
      MediaSourceOpenedCB;
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
                      const scoped_refptr<base::MessageLoopProxy>& media_loop,
                      media::MediaLog* media_log);

  // Initialize the MediaSourceDelegate. |media_source| will be owned by
  // this object after this call.
  void InitializeMediaSource(
      const MediaSourceOpenedCB& media_source_opened_cb,
      const media::NeedKeyCB& need_key_cb,
      const media::SetDecryptorReadyCB& set_decryptor_ready_cb,
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

  // Seeks the demuxer and acknowledges the seek request with |seek_request_id|
  // after the seek has been completed. This method can be called during pending
  // seeks, in which case only the last seek request will be acknowledged.
  void Seek(base::TimeDelta time, unsigned seek_request_id);

  void NotifyKeyAdded(const std::string& key_system);

  // Called when DemuxerStreamPlayer needs to read data from ChunkDemuxer.
  void OnReadFromDemuxer(media::DemuxerStream::Type type);

  // Called when the player needs the new config data from ChunkDemuxer.
  void OnMediaConfigRequest();

  // Called by the Destroyer to destroy an instance of this object.
  void Destroy();

 private:
  typedef base::Callback<void(
      scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> params)>
          ReadFromDemuxerAckCB;
  typedef base::Callback<void(
      scoped_ptr<media::MediaPlayerHostMsg_DemuxerReady_Params> params)>
          DemuxerReadyCB;

  // This is private to enforce use of the Destroyer.
  virtual ~MediaSourceDelegate();

  // Methods inherited from DemuxerHost.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void AddBufferedByteRange(int64 start, int64 end) OVERRIDE;
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE;
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void OnDemuxerError(media::PipelineStatus status) OVERRIDE;

  // Callback for ChunkDemuxer initialization.
  void OnDemuxerInitDone(media::PipelineStatus status);

  // Initializes DecryptingDemuxerStreams if audio/video stream is encrypted.
  void InitAudioDecryptingDemuxerStream();
  void InitVideoDecryptingDemuxerStream();

  // Callbacks for DecryptingDemuxerStream::Initialize().
  void OnAudioDecryptingDemuxerStreamInitDone(media::PipelineStatus status);
  void OnVideoDecryptingDemuxerStreamInitDone(media::PipelineStatus status);

  // Callback for Demuxer seek. It will call ResetAudioDecryptingDemuxerStream()
  // as part of the reset callback chain.
  void OnDemuxerSeekDone(unsigned seek_request_id,
                         media::PipelineStatus status);

  // Resets AudioDecryptingDemuxerStream if it exists. Then it will call
  // ResetVideoDecryptingDemuxerStream() as part of the reset callback chain.
  void ResetAudioDecryptingDemuxerStream();

  // Resets VideoDecryptingDemuxerStream if it exists. Then it will call
  // SendSeekRequestAck() as part of the reset callback chain.
  void ResetVideoDecryptingDemuxerStream();

  // Sends SeekRequestAck to the browser.
  void SendSeekRequestAck();

  void OnDemuxerStopDone();
  void OnDemuxerOpened();
  void OnNeedKey(const std::string& type,
                 const std::string& session_id,
                 const std::vector<uint8>& init_data);
  scoped_ptr<media::TextTrack> OnAddTextTrack(media::TextKind kind,
                                              const std::string& label,
                                              const std::string& language);
  void NotifyDemuxerReady();
  bool CanNotifyDemuxerReady();
  void SendDemuxerReady(
      scoped_ptr<media::MediaPlayerHostMsg_DemuxerReady_Params> params);

  void StopDemuxer();
  void InitializeDemuxer();
  void SeekInternal(base::TimeDelta time, unsigned seek_request_id);
  void OnReadFromDemuxerInternal(media::DemuxerStream::Type type);
  // Reads an access unit from the demuxer stream |stream| and stores it in
  // the |index|th access unit in |params|.
  void ReadFromDemuxerStream(
      media::DemuxerStream::Type type,
      scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> params,
      size_t index);
  void OnBufferReady(
      media::DemuxerStream::Type type,
      scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> params,
      size_t index,
      media::DemuxerStream::Status status,
      const scoped_refptr<media::DecoderBuffer>& buffer);

  void SendReadFromDemuxerAck(
      scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> params);

  // Helper function for calculating duration.
  int GetDurationMs();

  bool HasEncryptedStream();

  void SetSeeking(bool seeking);
  bool IsSeeking() const;

  // Weak pointer must be dereferenced and invalidated on the same thread.
  base::WeakPtrFactory<MediaSourceDelegate> main_weak_this_;
  base::WeakPtrFactory<MediaSourceDelegate> media_weak_this_;

  // Message loop for main renderer thread.
  const scoped_refptr<base::MessageLoopProxy> main_loop_;
#if defined(GOOGLE_TV)
  // Message loop for the media thread.
  // When there is high load in the render thread, the reading from |demuxer_|
  // and its read-callback loops run very slowly.  To improve the response time
  // of the readings, we run tasks related to |demuxer_| in the media thread.
  const scoped_refptr<base::MessageLoopProxy> media_loop_;

  ReadFromDemuxerAckCB send_read_from_demuxer_ack_cb_;
  base::Closure send_seek_request_ack_cb_;
  DemuxerReadyCB send_demuxer_ready_cb_;
#endif

  WebMediaPlayerProxyAndroid* proxy_;
  int player_id_;

  scoped_refptr<media::MediaLog> media_log_;
  UpdateNetworkStateCB update_network_state_cb_;
  DurationChangeCB duration_change_cb_;

  scoped_ptr<media::ChunkDemuxer> chunk_demuxer_;
  media::Demuxer* demuxer_;
  bool is_demuxer_ready_;

  media::SetDecryptorReadyCB set_decryptor_ready_cb_;

  scoped_ptr<media::DecryptingDemuxerStream> audio_decrypting_demuxer_stream_;
  scoped_ptr<media::DecryptingDemuxerStream> video_decrypting_demuxer_stream_;

  media::DemuxerStream* audio_stream_;
  media::DemuxerStream* video_stream_;

  media::PipelineStatistics statistics_;
  media::Ranges<base::TimeDelta> buffered_time_ranges_;
  // Keep a list of buffered time ranges.
  WebKit::WebTimeRanges buffered_web_time_ranges_;

  MediaSourceOpenedCB media_source_opened_cb_;
  media::NeedKeyCB need_key_cb_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  WebKit::WebString current_key_system_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // Lock used to serialize access for |seeking_|.
  mutable base::Lock seeking_lock_;
  bool seeking_;

  base::TimeDelta last_seek_time_;
  unsigned last_seek_request_id_;

  bool key_added_;
  std::string key_system_;

  size_t access_unit_size_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_
