// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_source_delegate.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "content/renderer/media/android/webmediaplayer_proxy_android.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/media/webmediasource_impl.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebMediaSource.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

using media::DemuxerStream;
using media::MediaPlayerHostMsg_DemuxerReady_Params;
using media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params;
using WebKit::WebMediaPlayer;
using WebKit::WebString;

namespace {

// The size of the access unit to transfer in an IPC in case of MediaSource.
// 16: approximately 250ms of content in 60 fps movies.
const size_t kAccessUnitSizeForMediaSource = 16;

const uint8 kVorbisPadding[] = { 0xff, 0xff, 0xff, 0xff };

}  // namespace

namespace content {

// TODO(xhwang): BIND_TO_RENDER_LOOP* force posts callback! Since everything
// works on the same thread in this class, not all force posts are necessary.

#define BIND_TO_RENDER_LOOP(function) \
  media::BindToLoop(base::MessageLoopProxy::current(), \
                    base::Bind(function, weak_this_.GetWeakPtr()))

#define BIND_TO_RENDER_LOOP_1(function, arg1) \
  media::BindToLoop(base::MessageLoopProxy::current(), \
                    base::Bind(function, weak_this_.GetWeakPtr(), arg1))

#define BIND_TO_RENDER_LOOP_2(function, arg1, arg2) \
  media::BindToLoop(base::MessageLoopProxy::current(), \
                    base::Bind(function, weak_this_.GetWeakPtr(), arg1, arg2))

#define BIND_TO_RENDER_LOOP_3(function, arg1, arg2, arg3) \
  media::BindToLoop(base::MessageLoopProxy::current(), \
                    base::Bind(function, \
                               weak_this_.GetWeakPtr(), arg1, arg2, arg3))

static void LogMediaSourceError(const scoped_refptr<media::MediaLog>& media_log,
                                const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}

MediaSourceDelegate::MediaSourceDelegate(WebMediaPlayerProxyAndroid* proxy,
                                         int player_id,
                                         media::MediaLog* media_log)
    : weak_this_(this),
      proxy_(proxy),
      player_id_(player_id),
      media_log_(media_log),
      demuxer_(NULL),
      is_demuxer_ready_(false),
      audio_stream_(NULL),
      video_stream_(NULL),
      seeking_(false),
      last_seek_request_id_(0),
      key_added_(false),
      access_unit_size_(0) {
}

MediaSourceDelegate::~MediaSourceDelegate() {
  DVLOG(1) << "~MediaSourceDelegate() : " << player_id_;
  DCHECK(!chunk_demuxer_);
  DCHECK(!demuxer_);
  DCHECK(!audio_decrypting_demuxer_stream_);
  DCHECK(!video_decrypting_demuxer_stream_);
  DCHECK(!audio_stream_);
  DCHECK(!video_stream_);
}

void MediaSourceDelegate::Destroy() {
  DVLOG(1) << "Destroy() : " << player_id_;
  if (!demuxer_) {
    delete this;
    return;
  }

  duration_change_cb_.Reset();
  update_network_state_cb_.Reset();
  media_source_opened_cb_.Reset();
  proxy_ = NULL;

  demuxer_ = NULL;
  audio_stream_ = NULL;
  video_stream_ = NULL;
  // TODO(xhwang): Figure out if we need to Reset the DDSs after Seeking or
  // before destroying them.
  audio_decrypting_demuxer_stream_.reset();
  video_decrypting_demuxer_stream_.reset();

  weak_this_.InvalidateWeakPtrs();
  DCHECK(!weak_this_.HasWeakPtrs());

  if (chunk_demuxer_) {
    // The callback OnDemuxerStopDone() owns |this| and will delete it when
    // called. Hence using base::Unretained(this) is safe here.
    chunk_demuxer_->Stop(base::Bind(&MediaSourceDelegate::OnDemuxerStopDone,
                                    base::Unretained(this)));
  }
}

void MediaSourceDelegate::InitializeMediaSource(
    const MediaSourceOpenedCB& media_source_opened_cb,
    const media::NeedKeyCB& need_key_cb,
    const media::SetDecryptorReadyCB& set_decryptor_ready_cb,
    const UpdateNetworkStateCB& update_network_state_cb,
    const DurationChangeCB& duration_change_cb) {
  DCHECK(!media_source_opened_cb.is_null());
  media_source_opened_cb_ = media_source_opened_cb;
  need_key_cb_ = need_key_cb;
  set_decryptor_ready_cb_ = set_decryptor_ready_cb;
  update_network_state_cb_ = update_network_state_cb;
  duration_change_cb_ = duration_change_cb;
  access_unit_size_ = kAccessUnitSizeForMediaSource;

  chunk_demuxer_.reset(new media::ChunkDemuxer(
      BIND_TO_RENDER_LOOP(&MediaSourceDelegate::OnDemuxerOpened),
      BIND_TO_RENDER_LOOP_1(&MediaSourceDelegate::OnNeedKey, ""),
      // WeakPtrs can only bind to methods without return values.
      base::Bind(&MediaSourceDelegate::OnAddTextTrack, base::Unretained(this)),
      base::Bind(&LogMediaSourceError, media_log_)));
  demuxer_ = chunk_demuxer_.get();

  chunk_demuxer_->Initialize(this,
      BIND_TO_RENDER_LOOP(&MediaSourceDelegate::OnDemuxerInitDone));
}

#if defined(GOOGLE_TV)
void MediaSourceDelegate::InitializeMediaStream(
    media::Demuxer* demuxer,
    const UpdateNetworkStateCB& update_network_state_cb) {
  DCHECK(demuxer);
  demuxer_ = demuxer;
  update_network_state_cb_ = update_network_state_cb;
  // When playing Media Stream, don't wait to accumulate multiple packets per
  // IPC communication.
  access_unit_size_ = 1;

  demuxer_->Initialize(this,
      BIND_TO_RENDER_LOOP(&MediaSourceDelegate::OnDemuxerInitDone));
}
#endif

const WebKit::WebTimeRanges& MediaSourceDelegate::Buffered() {
  buffered_web_time_ranges_ =
      ConvertToWebTimeRanges(buffered_time_ranges_);
  return buffered_web_time_ranges_;
}

size_t MediaSourceDelegate::DecodedFrameCount() const {
  return statistics_.video_frames_decoded;
}

size_t MediaSourceDelegate::DroppedFrameCount() const {
  return statistics_.video_frames_dropped;
}

size_t MediaSourceDelegate::AudioDecodedByteCount() const {
  return statistics_.audio_bytes_decoded;
}

size_t MediaSourceDelegate::VideoDecodedByteCount() const {
  return statistics_.video_bytes_decoded;
}

void MediaSourceDelegate::Seek(base::TimeDelta time, unsigned seek_request_id) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ") : " << player_id_;

  last_seek_time_ = time;
  last_seek_request_id_ = seek_request_id;

  if (chunk_demuxer_) {
    if (seeking_) {
      chunk_demuxer_->CancelPendingSeek(time);
      return;
    }

    chunk_demuxer_->StartWaitingForSeek(time);
  }

  seeking_ = true;
  demuxer_->Seek(time,
                 BIND_TO_RENDER_LOOP_1(&MediaSourceDelegate::OnDemuxerSeekDone,
                                       seek_request_id));
}

void MediaSourceDelegate::SetTotalBytes(int64 total_bytes) {
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::AddBufferedByteRange(int64 start, int64 end) {
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::AddBufferedTimeRange(base::TimeDelta start,
                                               base::TimeDelta end) {
  buffered_time_ranges_.Add(start, end);
}

void MediaSourceDelegate::SetDuration(base::TimeDelta duration) {
  DVLOG(1) << "SetDuration(" << duration.InSecondsF() << ") : " << player_id_;
  // Notify our owner (e.g. WebMediaPlayerAndroid) that
  // duration has changed.
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run(duration);
}

void MediaSourceDelegate::OnReadFromDemuxer(media::DemuxerStream::Type type) {
  DVLOG(1) << "OnReadFromDemuxer(" << type << ") : " << player_id_;
  if (seeking_)
    return;  // Drop the request during seeking.

  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO);
  // The access unit size should have been initialized properly at this stage.
  DCHECK_GT(access_unit_size_, 0u);
  MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params =
      type == DemuxerStream::AUDIO ? &audio_params_ : &video_params_;
  params->type = type;
  params->access_units.resize(access_unit_size_);
  ReadFromDemuxerStream(type, params, 0);
}

void MediaSourceDelegate::ReadFromDemuxerStream(
    media::DemuxerStream::Type type,
    MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params,
    size_t index) {
  DCHECK(!seeking_);
  // DemuxerStream::Read() always returns the read callback asynchronously.
  DemuxerStream* stream =
      (type == DemuxerStream::AUDIO) ? audio_stream_ : video_stream_;
  stream->Read(base::Bind(&MediaSourceDelegate::OnBufferReady,
                          weak_this_.GetWeakPtr(), type, params, index));
}

void MediaSourceDelegate::OnBufferReady(
    media::DemuxerStream::Type type,
    MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params,
    size_t index,
    DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  DVLOG(1) << "OnBufferReady(" << index << ", " << status << ", "
           << ((!buffer || buffer->end_of_stream()) ?
               -1 : buffer->timestamp().InMilliseconds())
           << ") : " << player_id_;
  DCHECK(demuxer_);

  // No new OnReadFromDemuxer() will be called during seeking. So this callback
  // must be from previous OnReadFromDemuxer() call and should be ignored.
  if (seeking_) {
    DVLOG(1) << "OnBufferReady(): Ignore previous read during seeking.";
    params->access_units.clear();
    return;
  }

  bool is_audio = (type == DemuxerStream::AUDIO);
  if (status != DemuxerStream::kAborted &&
      index >= params->access_units.size()) {
    LOG(ERROR) << "The internal state inconsistency onBufferReady: "
               << (is_audio ? "Audio" : "Video") << ", index " << index
               <<", size " << params->access_units.size()
               << ", status " << static_cast<int>(status);
    NOTREACHED();
    return;
  }

  switch (status) {
    case DemuxerStream::kAborted:
      // Because the abort was caused by the seek, don't respond ack.
      DVLOG(1) << "OnBufferReady() : Aborted";
      params->access_units.clear();
      return;

    case DemuxerStream::kConfigChanged:
      // In case of kConfigChanged, need to read decoder_config once
      // for the next reads.
      // TODO(kjyoun): Investigate if we need to use this new config. See
      // http://crbug.com/255783
      if (is_audio) {
        audio_stream_->audio_decoder_config();
      } else {
        gfx::Size size = video_stream_->video_decoder_config().coded_size();
        DVLOG(1) << "Video config is changed: " << size.width() << "x"
                 << size.height();
      }
      params->access_units[index].status = status;
      params->access_units.resize(index + 1);
      break;

    case DemuxerStream::kOk:
      params->access_units[index].status = status;
      if (buffer->end_of_stream()) {
        params->access_units[index].end_of_stream = true;
        params->access_units.resize(index + 1);
        break;
      }
      // TODO(ycheo): We assume that the inputed stream will be decoded
      // right away.
      // Need to implement this properly using MediaPlayer.OnInfoListener.
      if (is_audio) {
        statistics_.audio_bytes_decoded += buffer->data_size();
      } else {
        statistics_.video_bytes_decoded += buffer->data_size();
        statistics_.video_frames_decoded++;
      }
      params->access_units[index].timestamp = buffer->timestamp();
      params->access_units[index].data = std::vector<uint8>(
          buffer->data(),
          buffer->data() + buffer->data_size());
#if !defined(GOOGLE_TV)
      // Vorbis needs 4 extra bytes padding on Android. Check
      // NuMediaExtractor.cpp in Android source code.
      if (is_audio && media::kCodecVorbis ==
          audio_stream_->audio_decoder_config().codec()) {
        params->access_units[index].data.insert(
            params->access_units[index].data.end(), kVorbisPadding,
            kVorbisPadding + 4);
      }
#endif
      if (buffer->decrypt_config()) {
        params->access_units[index].key_id = std::vector<char>(
            buffer->decrypt_config()->key_id().begin(),
            buffer->decrypt_config()->key_id().end());
        params->access_units[index].iv = std::vector<char>(
            buffer->decrypt_config()->iv().begin(),
            buffer->decrypt_config()->iv().end());
        params->access_units[index].subsamples =
            buffer->decrypt_config()->subsamples();
      }
      if (++index < params->access_units.size()) {
        ReadFromDemuxerStream(type, params, index);
        return;
      }
      break;

    default:
      NOTREACHED();
  }

  if (proxy_)
    proxy_->ReadFromDemuxerAck(player_id_, *params);

  params->access_units.clear();
}

void MediaSourceDelegate::OnDemuxerError(media::PipelineStatus status) {
  DVLOG(1) << "OnDemuxerError(" << status << ") : " << player_id_;
  if (status != media::PIPELINE_OK && !update_network_state_cb_.is_null())
    update_network_state_cb_.Run(PipelineErrorToNetworkState(status));
}

void MediaSourceDelegate::OnDemuxerInitDone(media::PipelineStatus status) {
  DVLOG(1) << "OnDemuxerInitDone(" << status << ") : " << player_id_;
  DCHECK(demuxer_);

  if (status != media::PIPELINE_OK) {
    OnDemuxerError(status);
    return;
  }

  audio_stream_ = demuxer_->GetStream(DemuxerStream::AUDIO);
  video_stream_ = demuxer_->GetStream(DemuxerStream::VIDEO);

  if (audio_stream_ && audio_stream_->audio_decoder_config().is_encrypted() &&
      !set_decryptor_ready_cb_.is_null()) {
    InitAudioDecryptingDemuxerStream();
    // InitVideoDecryptingDemuxerStream() will be called in
    // OnAudioDecryptingDemuxerStreamInitDone().
    return;
  }

  if (video_stream_ && video_stream_->video_decoder_config().is_encrypted() &&
      !set_decryptor_ready_cb_.is_null()) {
    InitVideoDecryptingDemuxerStream();
    return;
  }

  // Notify demuxer ready when both streams are not encrypted.
  is_demuxer_ready_ = true;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::InitAudioDecryptingDemuxerStream() {
  DVLOG(1) << "InitAudioDecryptingDemuxerStream() : " << player_id_;
  DCHECK(!set_decryptor_ready_cb_.is_null());

  audio_decrypting_demuxer_stream_.reset(new media::DecryptingDemuxerStream(
      base::MessageLoopProxy::current(), set_decryptor_ready_cb_));
  audio_decrypting_demuxer_stream_->Initialize(
      audio_stream_,
      BIND_TO_RENDER_LOOP(
          &MediaSourceDelegate::OnAudioDecryptingDemuxerStreamInitDone));
}

void MediaSourceDelegate::InitVideoDecryptingDemuxerStream() {
  DVLOG(1) << "InitVideoDecryptingDemuxerStream() : " << player_id_;
  DCHECK(!set_decryptor_ready_cb_.is_null());

  video_decrypting_demuxer_stream_.reset(new media::DecryptingDemuxerStream(
      base::MessageLoopProxy::current(), set_decryptor_ready_cb_));
  video_decrypting_demuxer_stream_->Initialize(
      video_stream_,
      BIND_TO_RENDER_LOOP(
          &MediaSourceDelegate::OnVideoDecryptingDemuxerStreamInitDone));
}

void MediaSourceDelegate::OnAudioDecryptingDemuxerStreamInitDone(
    media::PipelineStatus status) {
  DVLOG(1) << "OnAudioDecryptingDemuxerStreamInitDone(" << status
           << ") : " << player_id_;
  DCHECK(demuxer_);

  if (status != media::PIPELINE_OK)
    audio_decrypting_demuxer_stream_.reset();
  else
    audio_stream_ = audio_decrypting_demuxer_stream_.get();

  if (video_stream_ && video_stream_->video_decoder_config().is_encrypted()) {
    InitVideoDecryptingDemuxerStream();
    return;
  }

  // Try to notify demuxer ready when audio DDS initialization finished and
  // video is not encrypted.
  is_demuxer_ready_ = true;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::OnVideoDecryptingDemuxerStreamInitDone(
    media::PipelineStatus status) {
  DVLOG(1) << "OnVideoDecryptingDemuxerStreamInitDone(" << status
           << ") : " << player_id_;
  DCHECK(demuxer_);

  if (status != media::PIPELINE_OK)
    video_decrypting_demuxer_stream_.reset();
  else
    video_stream_ = video_decrypting_demuxer_stream_.get();

  // Try to notify demuxer ready when video DDS initialization finished.
  is_demuxer_ready_ = true;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::OnDemuxerSeekDone(unsigned seek_request_id,
                                            media::PipelineStatus status) {
  DVLOG(1) << "OnDemuxerSeekDone(" << status << ") : " << player_id_;
  DCHECK(seeking_);

  if (status != media::PIPELINE_OK) {
    OnDemuxerError(status);
    return;
  }

  // Newer seek has been issued. Resume the last seek request.
  if (seek_request_id != last_seek_request_id_) {
    if (chunk_demuxer_)
      chunk_demuxer_->StartWaitingForSeek(last_seek_time_);
    demuxer_->Seek(
        last_seek_time_,
        BIND_TO_RENDER_LOOP_1(&MediaSourceDelegate::OnDemuxerSeekDone,
                              last_seek_request_id_));
    return;
  }

  ResetAudioDecryptingDemuxerStream();
}

void MediaSourceDelegate::ResetAudioDecryptingDemuxerStream() {
  DVLOG(1) << "ResetAudioDecryptingDemuxerStream() : " << player_id_;
  if (audio_decrypting_demuxer_stream_) {
    audio_decrypting_demuxer_stream_->Reset(
        base::Bind(&MediaSourceDelegate::ResetVideoDecryptingDemuxerStream,
                   weak_this_.GetWeakPtr()));
  } else {
    ResetVideoDecryptingDemuxerStream();
  }
}

void MediaSourceDelegate::ResetVideoDecryptingDemuxerStream() {
  DVLOG(1) << "ResetVideoDecryptingDemuxerStream()";
  if (video_decrypting_demuxer_stream_) {
    video_decrypting_demuxer_stream_->Reset(
        base::Bind(&MediaSourceDelegate::SendSeekRequestAck,
                   weak_this_.GetWeakPtr()));
  } else {
    SendSeekRequestAck();
  }
}

void MediaSourceDelegate::SendSeekRequestAck() {
  DVLOG(1) << "SendSeekRequestAck() : " << player_id_;
  seeking_ = false;
  proxy_->SeekRequestAck(player_id_, last_seek_request_id_);
  last_seek_request_id_ = 0;
}

void MediaSourceDelegate::OnDemuxerStopDone() {
  DVLOG(1) << "OnDemuxerStopDone() : " << player_id_;
  chunk_demuxer_.reset();
  delete this;
}

void MediaSourceDelegate::OnMediaConfigRequest() {
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::NotifyKeyAdded(const std::string& key_system) {
  DVLOG(1) << "NotifyKeyAdded() : " << player_id_;
  // TODO(kjyoun): Enhance logic to detect when to call NotifyDemuxerReady()
  // For now, we calls it when the first key is added. See
  // http://crbug.com/255781
  if (key_added_)
    return;
  key_added_ = true;
  key_system_ = key_system;
  if (!CanNotifyDemuxerReady())
    return;
  if (HasEncryptedStream())
    NotifyDemuxerReady();
}

bool MediaSourceDelegate::CanNotifyDemuxerReady() {
  // This can happen when a key is added before the demuxer is initialized.
  // See NotifyKeyAdded().
  // TODO(kjyoun): Remove NotifyDemxuerReady() call from NotifyKeyAdded() so
  // that we can remove all is_demuxer_ready_/key_added_/key_system_ madness.
  // See http://crbug.com/255781
  if (!is_demuxer_ready_)
    return false;
  if (HasEncryptedStream() && !key_added_)
    return false;
  return true;
}

void MediaSourceDelegate::NotifyDemuxerReady() {
  DVLOG(1) << "NotifyDemuxerReady() : " << player_id_;
  DCHECK(CanNotifyDemuxerReady());

  MediaPlayerHostMsg_DemuxerReady_Params params;
  if (audio_stream_) {
    media::AudioDecoderConfig config = audio_stream_->audio_decoder_config();
    params.audio_codec = config.codec();
    params.audio_channels =
        media::ChannelLayoutToChannelCount(config.channel_layout());
    params.audio_sampling_rate = config.samples_per_second();
    params.is_audio_encrypted = config.is_encrypted();
    params.audio_extra_data = std::vector<uint8>(
        config.extra_data(), config.extra_data() + config.extra_data_size());
  }
  if (video_stream_) {
    media::VideoDecoderConfig config = video_stream_->video_decoder_config();
    params.video_codec = config.codec();
    params.video_size = config.natural_size();
    params.is_video_encrypted = config.is_encrypted();
    params.video_extra_data = std::vector<uint8>(
        config.extra_data(), config.extra_data() + config.extra_data_size());
  }
  params.duration_ms = GetDurationMs();
  params.key_system = HasEncryptedStream() ? key_system_ : "";

  if (proxy_)
    proxy_->DemuxerReady(player_id_, params);
}

int MediaSourceDelegate::GetDurationMs() {
  if (!chunk_demuxer_)
    return -1;

  double duration_ms = chunk_demuxer_->GetDuration() * 1000;
  if (duration_ms > std::numeric_limits<int32>::max()) {
    LOG(WARNING) << "Duration from ChunkDemuxer is too large; probably "
                    "something has gone wrong.";
    return std::numeric_limits<int32>::max();
  }
  return duration_ms;
}

void MediaSourceDelegate::OnDemuxerOpened() {
  if (media_source_opened_cb_.is_null())
    return;

  media_source_opened_cb_.Run(new WebMediaSourceImpl(
      chunk_demuxer_.get(), base::Bind(&LogMediaSourceError, media_log_)));
}

void MediaSourceDelegate::OnNeedKey(const std::string& session_id,
                                    const std::string& type,
                                    scoped_ptr<uint8[]> init_data,
                                    int init_data_size) {
  if (need_key_cb_.is_null())
    return;

  need_key_cb_.Run(session_id, type, init_data.Pass(), init_data_size);
}

scoped_ptr<media::TextTrack> MediaSourceDelegate::OnAddTextTrack(
    media::TextKind kind,
    const std::string& label,
    const std::string& language) {
  return scoped_ptr<media::TextTrack>();
}

bool MediaSourceDelegate::HasEncryptedStream() {
  return (audio_stream_ &&
          audio_stream_->audio_decoder_config().is_encrypted()) ||
         (video_stream_ &&
          video_stream_->video_decoder_config().is_encrypted());
}

}  // namespace content
