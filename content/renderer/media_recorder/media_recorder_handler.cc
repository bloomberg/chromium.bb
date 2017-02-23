// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_recorder/media_recorder_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/media_recorder/audio_track_recorder.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mime_util.h"
#include "media/base/video_frame.h"
#include "media/muxers/webm_muxer.h"
#include "third_party/WebKit/public/platform/WebMediaRecorderHandlerClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebString.h"

using base::TimeDelta;
using base::TimeTicks;
using base::ToLowerASCII;

namespace content {

namespace {

media::VideoCodec CodecIdToMediaVideoCodec(VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::VP8:
      return media::kCodecVP8;
    case VideoTrackRecorder::CodecId::VP9:
      return media::kCodecVP9;
#if BUILDFLAG(RTC_USE_H264)
    case VideoTrackRecorder::CodecId::H264:
      return media::kCodecH264;
#endif
    case VideoTrackRecorder::CodecId::LAST:
      return media::kUnknownVideoCodec;
  }
  NOTREACHED() << "Unsupported codec";
  return media::kUnknownVideoCodec;
}

}  // anonymous namespace

MediaRecorderHandler::MediaRecorderHandler()
    : video_bits_per_second_(0),
      audio_bits_per_second_(0),
      codec_id_(VideoTrackRecorder::CodecId::VP8),
      recording_(false),
      client_(nullptr),
      weak_factory_(this) {}

MediaRecorderHandler::~MediaRecorderHandler() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Send a |last_in_slice| to our |client_|.
  if (client_)
    client_->writeData(
        nullptr, 0u, true,
        (TimeTicks::Now() - TimeTicks::UnixEpoch()).InMillisecondsF());
}

bool MediaRecorderHandler::canSupportMimeType(
    const blink::WebString& web_type,
    const blink::WebString& web_codecs) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // An empty |web_type| means MediaRecorderHandler can choose its preferred
  // codecs.
  if (web_type.isEmpty())
    return true;

  const std::string type(web_type.utf8());
  const bool video = base::EqualsCaseInsensitiveASCII(type, "video/webm") ||
                     base::EqualsCaseInsensitiveASCII(type, "video/x-matroska");
  const bool audio =
      video ? false : base::EqualsCaseInsensitiveASCII(type, "audio/webm");
  if (!video && !audio)
    return false;

  // Both |video| and |audio| support empty |codecs|; |type| == "video" supports
  // vp8, vp9, h264 and avc1 or opus; |type| = "audio", supports only opus.
  // http://www.webmproject.org/docs/container Sec:"HTML5 Video Type Parameters"
  static const char* const kVideoCodecs[] = {"vp8", "vp9", "h264", "avc1",
                                             "opus"};
  static const char* const kAudioCodecs[] = { "opus" };
  const char* const* codecs = video ? &kVideoCodecs[0] : &kAudioCodecs[0];
  const int codecs_count =
      video ? arraysize(kVideoCodecs) : arraysize(kAudioCodecs);

  std::vector<std::string> codecs_list;
  media::SplitCodecsToVector(web_codecs.utf8(), &codecs_list, true /* strip */);
  for (const auto& codec : codecs_list) {
    auto* const* found = std::find_if(
        &codecs[0], &codecs[codecs_count], [&codec](const char* name) {
          return base::EqualsCaseInsensitiveASCII(codec, name);
        });
    if (found == &codecs[codecs_count])
      return false;
  }
  return true;
}

bool MediaRecorderHandler::initialize(
    blink::WebMediaRecorderHandlerClient* client,
    const blink::WebMediaStream& media_stream,
    const blink::WebString& type,
    const blink::WebString& codecs,
    int32_t audio_bits_per_second,
    int32_t video_bits_per_second) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_MEDIA_STREAM_RECORDER);

  if (!canSupportMimeType(type, codecs)) {
    DLOG(ERROR) << "Unsupported " << type.utf8() << ";codecs=" << codecs.utf8();
    return false;
  }

  // Once established that we support the codec(s), hunt then individually.
  const std::string& codecs_str = ToLowerASCII(codecs.utf8());
  if (codecs_str.find("vp8") != std::string::npos)
    codec_id_ = VideoTrackRecorder::CodecId::VP8;
  else if (codecs_str.find("vp9") != std::string::npos)
    codec_id_ = VideoTrackRecorder::CodecId::VP9;
#if BUILDFLAG(RTC_USE_H264)
  else if (codecs_str.find("h264") != std::string::npos)
    codec_id_ = VideoTrackRecorder::CodecId::H264;
  else if (codecs_str.find("avc1") != std::string::npos)
    codec_id_ = VideoTrackRecorder::CodecId::H264;
#endif
  else
    codec_id_ = VideoTrackRecorder::GetPreferredCodecId();

  DVLOG_IF(1, codecs_str.empty()) << "Falling back to preferred codec id "
                                  << static_cast<int>(codec_id_);

  media_stream_ = media_stream;
  DCHECK(client);
  client_ = client;

  audio_bits_per_second_ = audio_bits_per_second;
  video_bits_per_second_ = video_bits_per_second;
  return true;
}

bool MediaRecorderHandler::start(int timeslice) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  DCHECK(!media_stream_.isNull());
  DCHECK(timeslice_.is_zero());
  DCHECK(!webm_muxer_);

  timeslice_ = TimeDelta::FromMilliseconds(timeslice);
  slice_origin_timestamp_ = TimeTicks::Now();

  media_stream_.videoTracks(video_tracks_);
  media_stream_.audioTracks(audio_tracks_);

  if (video_tracks_.isEmpty() && audio_tracks_.isEmpty()) {
    LOG(WARNING) << __func__ << ": no media tracks.";
    return false;
  }

  const bool use_video_tracks =
      !video_tracks_.isEmpty() && video_tracks_[0].isEnabled() &&
      video_tracks_[0].source().getReadyState() !=
          blink::WebMediaStreamSource::ReadyStateEnded;
  const bool use_audio_tracks =
      !audio_tracks_.isEmpty() &&
      MediaStreamAudioTrack::From(audio_tracks_[0]) &&
      audio_tracks_[0].isEnabled() &&
      audio_tracks_[0].source().getReadyState() !=
          blink::WebMediaStreamSource::ReadyStateEnded;

  if (!use_video_tracks && !use_audio_tracks) {
    LOG(WARNING) << __func__ << ": no tracks to be recorded.";
    return false;
  }

  webm_muxer_.reset(new media::WebmMuxer(
      CodecIdToMediaVideoCodec(codec_id_), use_video_tracks, use_audio_tracks,
      base::Bind(&MediaRecorderHandler::WriteData,
                 weak_factory_.GetWeakPtr())));

  if (use_video_tracks) {
    // TODO(mcasas): The muxer API supports only one video track. Extend it to
    // several video tracks, see http://crbug.com/528523.
    LOG_IF(WARNING, video_tracks_.size() > 1u)
        << "Recording multiple video tracks is not implemented. "
        << "Only recording first video track.";
    const blink::WebMediaStreamTrack& video_track = video_tracks_[0];
    if (video_track.isNull())
      return false;

    const VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb =
        media::BindToCurrentLoop(base::Bind(
            &MediaRecorderHandler::OnEncodedVideo, weak_factory_.GetWeakPtr()));

    video_recorders_.emplace_back(new VideoTrackRecorder(
        codec_id_, video_track, on_encoded_video_cb, video_bits_per_second_));
  }

  if (use_audio_tracks) {
    // TODO(ajose): The muxer API supports only one audio track. Extend it to
    // several tracks.
    LOG_IF(WARNING, audio_tracks_.size() > 1u)
        << "Recording multiple audio"
        << " tracks is not implemented.  Only recording first audio track.";
    const blink::WebMediaStreamTrack& audio_track = audio_tracks_[0];
    if (audio_track.isNull())
      return false;

    const AudioTrackRecorder::OnEncodedAudioCB on_encoded_audio_cb =
        media::BindToCurrentLoop(base::Bind(
            &MediaRecorderHandler::OnEncodedAudio, weak_factory_.GetWeakPtr()));

    audio_recorders_.emplace_back(new AudioTrackRecorder(
        audio_track, on_encoded_audio_cb, audio_bits_per_second_));
  }

  recording_ = true;
  return true;
}

void MediaRecorderHandler::stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Don't check |recording_| since we can go directly from pause() to stop().

  weak_factory_.InvalidateWeakPtrs();
  recording_ = false;
  timeslice_ = TimeDelta::FromMilliseconds(0);
  video_recorders_.clear();
  audio_recorders_.clear();
  webm_muxer_.reset();
}

void MediaRecorderHandler::pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(recording_);
  recording_ = false;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Pause();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Pause();
  webm_muxer_->Pause();
}

void MediaRecorderHandler::resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  recording_ = true;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Resume();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Resume();
  webm_muxer_->Resume();
}

void MediaRecorderHandler::OnEncodedVideo(
    const media::WebmMuxer::VideoParameters& params,
    std::unique_ptr<std::string> encoded_data,
    TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  if (UpdateTracksAndCheckIfChanged()) {
    client_->onError("Amount of tracks in MediaStream has changed.");
    return;
  }
  if (!webm_muxer_)
    return;
  if (!webm_muxer_->OnEncodedVideo(params, std::move(encoded_data), timestamp,
                                   is_key_frame)) {
    DLOG(ERROR) << "Error muxing video data";
    client_->onError("Error muxing video data");
  }
}

void MediaRecorderHandler::OnEncodedAudio(
    const media::AudioParameters& params,
    std::unique_ptr<std::string> encoded_data,
    base::TimeTicks timestamp) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  if (UpdateTracksAndCheckIfChanged()) {
    client_->onError("Amount of tracks in MediaStream has changed.");
    return;
  }
  if (!webm_muxer_)
    return;
  if (!webm_muxer_->OnEncodedAudio(params, std::move(encoded_data),
                                   timestamp)) {
    DLOG(ERROR) << "Error muxing audio data";
    client_->onError("Error muxing audio data");
  }
}

void MediaRecorderHandler::WriteData(base::StringPiece data) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  const TimeTicks now = TimeTicks::Now();
  // Non-buffered mode does not need to check timestamps.
  if (timeslice_.is_zero()) {
    client_->writeData(data.data(), data.length(), true /* lastInSlice */,
                       (now - TimeTicks::UnixEpoch()).InMillisecondsF());
    return;
  }

  const bool last_in_slice = now > slice_origin_timestamp_ + timeslice_;
  DVLOG_IF(1, last_in_slice) << "Slice finished @ " << now;
  if (last_in_slice)
    slice_origin_timestamp_ = now;
  client_->writeData(data.data(), data.length(), last_in_slice,
                     (now - TimeTicks::UnixEpoch()).InMillisecondsF());
}

bool MediaRecorderHandler::UpdateTracksAndCheckIfChanged() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks, audio_tracks;
  media_stream_.videoTracks(video_tracks);
  media_stream_.audioTracks(audio_tracks);

  bool video_tracks_changed = video_tracks_.size() != video_tracks.size();
  bool audio_tracks_changed = audio_tracks_.size() != audio_tracks.size();

  if (!video_tracks_changed) {
    for (size_t i = 0; i < video_tracks.size(); ++i) {
      if (video_tracks_[i].id() != video_tracks[i].id()) {
        video_tracks_changed = true;
        break;
      }
    }
  }
  if (!video_tracks_changed && !audio_tracks_changed) {
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      if (audio_tracks_[i].id() != audio_tracks[i].id()) {
        audio_tracks_changed = true;
        break;
      }
    }
  }

  if (video_tracks_changed)
    video_tracks_ = video_tracks;
  if (audio_tracks_changed)
    audio_tracks_ = audio_tracks;

  return video_tracks_changed || audio_tracks_changed;
}

void MediaRecorderHandler::OnVideoFrameForTesting(
    const scoped_refptr<media::VideoFrame>& frame,
    const TimeTicks& timestamp) {
  for (const auto& recorder : video_recorders_)
    recorder->OnVideoFrameForTesting(frame, timestamp);
}

void MediaRecorderHandler::OnAudioBusForTesting(
    const media::AudioBus& audio_bus,
    const base::TimeTicks& timestamp) {
  for (const auto& recorder : audio_recorders_)
    recorder->OnData(audio_bus, timestamp);
}

void MediaRecorderHandler::SetAudioFormatForTesting(
    const media::AudioParameters& params) {
  for (const auto& recorder : audio_recorders_)
    recorder->OnSetFormat(params);
}

}  // namespace content
