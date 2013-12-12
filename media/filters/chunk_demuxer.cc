// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include <algorithm>
#include <deque>
#include <limits>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/stream_parser_factory.h"

using base::TimeDelta;

namespace media {

// Contains state belonging to a source id.
class SourceState {
 public:
  // Callback signature used to create ChunkDemuxerStreams.
  typedef base::Callback<ChunkDemuxerStream*(
      DemuxerStream::Type)> CreateDemuxerStreamCB;

  // Callback signature used to notify ChunkDemuxer of timestamps
  // that may cause the duration to be updated.
  typedef base::Callback<void(
      TimeDelta, ChunkDemuxerStream*)> IncreaseDurationCB;

  typedef base::Callback<void(
      ChunkDemuxerStream*, const TextTrackConfig&)> NewTextTrackCB;

  SourceState(scoped_ptr<StreamParser> stream_parser, const LogCB& log_cb,
              const CreateDemuxerStreamCB& create_demuxer_stream_cb,
              const IncreaseDurationCB& increase_duration_cb);

  ~SourceState();

  void Init(const StreamParser::InitCB& init_cb,
            bool allow_audio,
            bool allow_video,
            const StreamParser::NeedKeyCB& need_key_cb,
            const NewTextTrackCB& new_text_track_cb);

  // Appends new data to the StreamParser.
  // Returns true if the data was successfully appended. Returns false if an
  // error occurred.
  bool Append(const uint8* data, size_t length);

  // Aborts the current append sequence and resets the parser.
  void Abort();

  // Sets |timestamp_offset_| if possible.
  // Returns if the offset was set. Returns false if the offset could not be
  // updated at this time.
  bool SetTimestampOffset(TimeDelta timestamp_offset);

  TimeDelta timestamp_offset() const { return timestamp_offset_; }

  void set_append_window_start(TimeDelta start) {
    append_window_start_ = start;
  }
  void set_append_window_end(TimeDelta end) { append_window_end_ = end; }

  void StartReturningData();
  void AbortReads();
  void Seek(TimeDelta seek_time);
  void CompletePendingReadIfPossible();

 private:
  // Called by the |stream_parser_| when a new initialization segment is
  // encountered.
  // Returns true on a successful call. Returns false if an error occured while
  // processing decoder configurations.
  bool OnNewConfigs(bool allow_audio, bool allow_video,
                    const AudioDecoderConfig& audio_config,
                    const VideoDecoderConfig& video_config,
                    const StreamParser::TextTrackConfigMap& text_configs);

  // Called by the |stream_parser_| at the beginning of a new media segment.
  void OnNewMediaSegment();

  // Called by the |stream_parser_| at the end of a media segment.
  void OnEndOfMediaSegment();

  // Called by the |stream_parser_| when new buffers have been parsed. It
  // applies |timestamp_offset_| to all buffers in |audio_buffers| and
  // |video_buffers| and then calls Append() on |audio_| and/or
  // |video_| with the modified buffers.
  // Returns true on a successful call. Returns false if an error occured while
  // processing the buffers.
  bool OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                    const StreamParser::BufferQueue& video_buffers);

  // Called by the |stream_parser_| when new text buffers have been parsed. It
  // applies |timestamp_offset_| to all buffers in |buffers| and then appends
  // the (modified) buffers to the demuxer stream associated with
  // the track having |text_track_number|.
  // Returns true on a successful call. Returns false if an error occured while
  // processing the buffers.
  bool OnTextBuffers(int text_track_number,
                     const StreamParser::BufferQueue& buffers);

  // Helper function that adds |timestamp_offset_| to each buffer in |buffers|.
  void AdjustBufferTimestamps(const StreamParser::BufferQueue& buffers);

  // Filters out buffers that are outside of the append window
  // [|append_window_start_|, |append_window_end_|).
  // |needs_keyframe| is a pointer to the |xxx_need_keyframe_| flag
  // associated with the |buffers|. Its state is read an updated as
  // this method filters |buffers|.
  // Buffers that are inside the append window are appended to the end
  // of |filtered_buffers|.
  void FilterWithAppendWindow(const StreamParser::BufferQueue& buffers,
                              bool* needs_keyframe,
                              StreamParser::BufferQueue* filtered_buffers);

  CreateDemuxerStreamCB create_demuxer_stream_cb_;
  IncreaseDurationCB increase_duration_cb_;
  NewTextTrackCB new_text_track_cb_;

  // The offset to apply to media segment timestamps.
  TimeDelta timestamp_offset_;

  TimeDelta append_window_start_;
  TimeDelta append_window_end_;

  // Set to true if the next buffers appended within the append window
  // represent the start of a new media segment. This flag being set
  // triggers a call to |new_segment_cb_| when the new buffers are
  // appended. The flag is set on actual media segment boundaries and
  // when the "append window" filtering causes discontinuities in the
  // appended data.
  bool new_media_segment_;

  // Keeps track of whether |timestamp_offset_| can be modified.
  bool can_update_offset_;

  // The object used to parse appended data.
  scoped_ptr<StreamParser> stream_parser_;

  ChunkDemuxerStream* audio_;
  bool audio_needs_keyframe_;

  ChunkDemuxerStream* video_;
  bool video_needs_keyframe_;

  typedef std::map<int, ChunkDemuxerStream*> TextStreamMap;
  TextStreamMap text_stream_map_;

  LogCB log_cb_;

  DISALLOW_COPY_AND_ASSIGN(SourceState);
};

class ChunkDemuxerStream : public DemuxerStream {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  explicit ChunkDemuxerStream(Type type);
  virtual ~ChunkDemuxerStream();

  // ChunkDemuxerStream control methods.
  void StartReturningData();
  void AbortReads();
  void CompletePendingReadIfPossible();
  void Shutdown();

  // SourceBufferStream manipulation methods.
  void Seek(TimeDelta time);
  bool IsSeekWaitingForData() const;

  // Add buffers to this stream.  Buffers are stored in SourceBufferStreams,
  // which handle ordering and overlap resolution.
  // Returns true if buffers were successfully added.
  bool Append(const StreamParser::BufferQueue& buffers);

  // Removes buffers between |start| and |end| according to the steps
  // in the "Coded Frame Removal Algorithm" in the Media Source
  // Extensions Spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-coded-frame-removal
  //
  // |duration| is the current duration of the presentation. It is
  // required by the computation outlined in the spec.
  void Remove(TimeDelta start, TimeDelta end, TimeDelta duration);

  // Signal to the stream that duration has changed to |duration|.
  void OnSetDuration(TimeDelta duration);

  // Returns the range of buffered data in this stream, capped at |duration|.
  Ranges<TimeDelta> GetBufferedRanges(TimeDelta duration) const;

  // Signal to the stream that buffers handed in through subsequent calls to
  // Append() belong to a media segment that starts at |start_timestamp|.
  void OnNewMediaSegment(TimeDelta start_timestamp);

  // Called when midstream config updates occur.
  // Returns true if the new config is accepted.
  // Returns false if the new config should trigger an error.
  bool UpdateAudioConfig(const AudioDecoderConfig& config, const LogCB& log_cb);
  bool UpdateVideoConfig(const VideoDecoderConfig& config, const LogCB& log_cb);
  void UpdateTextConfig(const TextTrackConfig& config, const LogCB& log_cb);

  void MarkEndOfStream();
  void UnmarkEndOfStream();

  // DemuxerStream methods.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;
  virtual AudioDecoderConfig audio_decoder_config() OVERRIDE;
  virtual VideoDecoderConfig video_decoder_config() OVERRIDE;

  // Returns the text track configuration.  It is an error to call this method
  // if type() != TEXT.
  TextTrackConfig text_track_config();

  void set_memory_limit_for_testing(int memory_limit) {
    stream_->set_memory_limit_for_testing(memory_limit);
  }

 private:
  enum State {
    UNINITIALIZED,
    RETURNING_DATA_FOR_READS,
    RETURNING_ABORT_FOR_READS,
    SHUTDOWN,
  };

  // Assigns |state_| to |state|
  void ChangeState_Locked(State state);

  void CompletePendingReadIfPossible_Locked();

  // Gets the value to pass to the next Read() callback. Returns true if
  // |status| and |buffer| should be passed to the callback. False indicates
  // that |status| and |buffer| were not set and more data is needed.
  bool GetNextBuffer_Locked(DemuxerStream::Status* status,
                            scoped_refptr<StreamParserBuffer>* buffer);

  // Specifies the type of the stream.
  Type type_;

  scoped_ptr<SourceBufferStream> stream_;

  mutable base::Lock lock_;
  State state_;
  ReadCB read_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

SourceState::SourceState(scoped_ptr<StreamParser> stream_parser,
                         const LogCB& log_cb,
                         const CreateDemuxerStreamCB& create_demuxer_stream_cb,
                         const IncreaseDurationCB& increase_duration_cb)
    : create_demuxer_stream_cb_(create_demuxer_stream_cb),
      increase_duration_cb_(increase_duration_cb),
      append_window_end_(kInfiniteDuration()),
      new_media_segment_(false),
      can_update_offset_(true),
      stream_parser_(stream_parser.release()),
      audio_(NULL),
      audio_needs_keyframe_(true),
      video_(NULL),
      video_needs_keyframe_(true),
      log_cb_(log_cb) {
  DCHECK(!create_demuxer_stream_cb_.is_null());
  DCHECK(!increase_duration_cb_.is_null());
}

SourceState::~SourceState() {
  if (audio_)
    audio_->Shutdown();

  if (video_)
    video_->Shutdown();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->Shutdown();
    delete itr->second;
  }
}

void SourceState::Init(const StreamParser::InitCB& init_cb,
                       bool allow_audio,
                       bool allow_video,
                       const StreamParser::NeedKeyCB& need_key_cb,
                       const NewTextTrackCB& new_text_track_cb) {
  new_text_track_cb_ = new_text_track_cb;

  StreamParser::NewTextBuffersCB new_text_buffers_cb;

  if (!new_text_track_cb_.is_null()) {
    new_text_buffers_cb = base::Bind(&SourceState::OnTextBuffers,
                                     base::Unretained(this));
  }

  stream_parser_->Init(init_cb,
                       base::Bind(&SourceState::OnNewConfigs,
                                  base::Unretained(this),
                                  allow_audio,
                                  allow_video),
                       base::Bind(&SourceState::OnNewBuffers,
                                  base::Unretained(this)),
                       new_text_buffers_cb,
                       need_key_cb,
                       base::Bind(&SourceState::OnNewMediaSegment,
                                  base::Unretained(this)),
                       base::Bind(&SourceState::OnEndOfMediaSegment,
                                  base::Unretained(this)),
                       log_cb_);
}

bool SourceState::SetTimestampOffset(TimeDelta timestamp_offset) {
  if (!can_update_offset_)
    return false;

  timestamp_offset_ = timestamp_offset;
  return true;
}

bool SourceState::Append(const uint8* data, size_t length) {
  return stream_parser_->Parse(data, length);
}

void SourceState::Abort() {
  stream_parser_->Flush();
  audio_needs_keyframe_ = true;
  video_needs_keyframe_ = true;
  can_update_offset_ = true;
}


void SourceState::StartReturningData() {
  if (audio_)
    audio_->StartReturningData();

  if (video_)
    video_->StartReturningData();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->StartReturningData();
  }
}

void SourceState::AbortReads() {
  if (audio_)
    audio_->AbortReads();

  if (video_)
    video_->AbortReads();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->AbortReads();
  }
}

void SourceState::Seek(TimeDelta seek_time) {
  if (audio_)
    audio_->Seek(seek_time);

  if (video_)
    video_->Seek(seek_time);

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->Seek(seek_time);
  }
}

void SourceState::CompletePendingReadIfPossible() {
  if (audio_)
    audio_->CompletePendingReadIfPossible();

  if (video_)
    video_->CompletePendingReadIfPossible();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->CompletePendingReadIfPossible();
  }
}

void SourceState::AdjustBufferTimestamps(
    const StreamParser::BufferQueue& buffers) {
  if (timestamp_offset_ == TimeDelta())
    return;

  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetDecodeTimestamp(
        (*itr)->GetDecodeTimestamp() + timestamp_offset_);
    (*itr)->set_timestamp((*itr)->timestamp() + timestamp_offset_);
  }
}

bool SourceState::OnNewConfigs(
    bool allow_audio, bool allow_video,
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config,
    const StreamParser::TextTrackConfigMap& text_configs) {
  DVLOG(1) << "OnNewConfigs(" << allow_audio << ", " << allow_video
           << ", " << audio_config.IsValidConfig()
           << ", " << video_config.IsValidConfig() << ")";

  if (!audio_config.IsValidConfig() && !video_config.IsValidConfig()) {
    DVLOG(1) << "OnNewConfigs() : Audio & video config are not valid!";
    return false;
  }

  // Signal an error if we get configuration info for stream types that weren't
  // specified in AddId() or more configs after a stream is initialized.
  if (allow_audio != audio_config.IsValidConfig()) {
    MEDIA_LOG(log_cb_)
        << "Initialization segment"
        << (audio_config.IsValidConfig() ? " has" : " does not have")
        << " an audio track, but the mimetype"
        << (allow_audio ? " specifies" : " does not specify")
        << " an audio codec.";
    return false;
  }

  if (allow_video != video_config.IsValidConfig()) {
    MEDIA_LOG(log_cb_)
        << "Initialization segment"
        << (video_config.IsValidConfig() ? " has" : " does not have")
        << " a video track, but the mimetype"
        << (allow_video ? " specifies" : " does not specify")
        << " a video codec.";
    return false;
  }

  bool success = true;
  if (audio_config.IsValidConfig()) {
    if (!audio_) {
      audio_ = create_demuxer_stream_cb_.Run(DemuxerStream::AUDIO);

      if (!audio_) {
        DVLOG(1) << "Failed to create an audio stream.";
        return false;
      }
    }

    success &= audio_->UpdateAudioConfig(audio_config, log_cb_);
  }

  if (video_config.IsValidConfig()) {
    if (!video_) {
      video_ = create_demuxer_stream_cb_.Run(DemuxerStream::VIDEO);

      if (!video_) {
        DVLOG(1) << "Failed to create a video stream.";
        return false;
      }
    }

    success &= video_->UpdateVideoConfig(video_config, log_cb_);
  }

  typedef StreamParser::TextTrackConfigMap::const_iterator TextConfigItr;
  if (text_stream_map_.empty()) {
    for (TextConfigItr itr = text_configs.begin();
         itr != text_configs.end(); ++itr) {
      ChunkDemuxerStream* const text_stream =
          create_demuxer_stream_cb_.Run(DemuxerStream::TEXT);
      text_stream->UpdateTextConfig(itr->second, log_cb_);
      text_stream_map_[itr->first] = text_stream;
      new_text_track_cb_.Run(text_stream, itr->second);
    }
  } else {
    const size_t text_count = text_stream_map_.size();
    if (text_configs.size() != text_count) {
      success &= false;
      MEDIA_LOG(log_cb_) << "The number of text track configs changed.";
    } else if (text_count == 1) {
      TextConfigItr config_itr = text_configs.begin();
      const TextTrackConfig& new_config = config_itr->second;
      TextStreamMap::iterator stream_itr = text_stream_map_.begin();
      ChunkDemuxerStream* text_stream = stream_itr->second;
      TextTrackConfig old_config = text_stream->text_track_config();
      if (!new_config.Matches(old_config)) {
        success &= false;
        MEDIA_LOG(log_cb_) << "New text track config does not match old one.";
      } else {
        text_stream_map_.clear();
        text_stream_map_[config_itr->first] = text_stream;
      }
    } else {
      for (TextConfigItr config_itr = text_configs.begin();
           config_itr != text_configs.end(); ++config_itr) {
        TextStreamMap::iterator stream_itr =
            text_stream_map_.find(config_itr->first);
        if (stream_itr == text_stream_map_.end()) {
          success &= false;
          MEDIA_LOG(log_cb_) << "Unexpected text track configuration "
                                "for track ID "
                             << config_itr->first;
          break;
        }

        const TextTrackConfig& new_config = config_itr->second;
        ChunkDemuxerStream* stream = stream_itr->second;
        TextTrackConfig old_config = stream->text_track_config();
        if (!new_config.Matches(old_config)) {
          success &= false;
          MEDIA_LOG(log_cb_) << "New text track config for track ID "
                             << config_itr->first
                             << " does not match old one.";
          break;
        }
      }
    }
  }

  DVLOG(1) << "OnNewConfigs() : " << (success ? "success" : "failed");
  return success;
}

void SourceState::OnNewMediaSegment() {
  DVLOG(2) << "OnNewMediaSegment()";
  can_update_offset_ = false;
  new_media_segment_ = true;
}

void SourceState::OnEndOfMediaSegment() {
  DVLOG(2) << "OnEndOfMediaSegment()";
  can_update_offset_ = true;
  new_media_segment_ = false;
}

bool SourceState::OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                               const StreamParser::BufferQueue& video_buffers) {
  DCHECK(!audio_buffers.empty() || !video_buffers.empty());
  AdjustBufferTimestamps(audio_buffers);
  AdjustBufferTimestamps(video_buffers);

  StreamParser::BufferQueue filtered_audio;
  StreamParser::BufferQueue filtered_video;

  FilterWithAppendWindow(audio_buffers, &audio_needs_keyframe_,
                         &filtered_audio);

  FilterWithAppendWindow(video_buffers, &video_needs_keyframe_,
                         &filtered_video);

  if (filtered_audio.empty() && filtered_video.empty())
    return true;

  if (new_media_segment_) {
    // Find the earliest timestamp in the filtered buffers and use that for the
    // segment start timestamp.
    TimeDelta segment_timestamp = kNoTimestamp();

    if (!filtered_audio.empty())
      segment_timestamp = filtered_audio.front()->GetDecodeTimestamp();

    if (!filtered_video.empty() &&
        (segment_timestamp == kNoTimestamp() ||
         filtered_video.front()->GetDecodeTimestamp() < segment_timestamp)) {
      segment_timestamp = filtered_video.front()->GetDecodeTimestamp();
    }

    new_media_segment_ = false;

    if (audio_)
      audio_->OnNewMediaSegment(segment_timestamp);

    if (video_)
      video_->OnNewMediaSegment(segment_timestamp);

    for (TextStreamMap::iterator itr = text_stream_map_.begin();
         itr != text_stream_map_.end(); ++itr) {
      itr->second->OnNewMediaSegment(segment_timestamp);
    }
  }

  if (!filtered_audio.empty()) {
    if (!audio_ || !audio_->Append(filtered_audio))
      return false;
    increase_duration_cb_.Run(filtered_audio.back()->timestamp(), audio_);
  }

  if (!filtered_video.empty()) {
    if (!video_ || !video_->Append(filtered_video))
      return false;
    increase_duration_cb_.Run(filtered_video.back()->timestamp(), video_);
  }

  return true;
}

bool SourceState::OnTextBuffers(
    int text_track_number,
    const StreamParser::BufferQueue& buffers) {
  DCHECK(!buffers.empty());

  TextStreamMap::iterator itr = text_stream_map_.find(text_track_number);
  if (itr == text_stream_map_.end())
    return false;

  AdjustBufferTimestamps(buffers);

  return itr->second->Append(buffers);
}

void SourceState::FilterWithAppendWindow(
    const StreamParser::BufferQueue& buffers, bool* needs_keyframe,
    StreamParser::BufferQueue* filtered_buffers) {
  DCHECK(needs_keyframe);
  DCHECK(filtered_buffers);

  // This loop implements steps 1.9, 1.10, & 1.11 of the "Coded frame
  // processing loop" in the Media Source Extensions spec.
  // These steps filter out buffers that are not within the "append
  // window" and handles resyncing on the next random access point
  // (i.e., next keyframe) if a buffer gets dropped.
  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    // Filter out buffers that are outside the append window. Anytime
    // a buffer gets dropped we need to set |*needs_keyframe| to true
    // because we can only resume decoding at keyframes.
    TimeDelta presentation_timestamp = (*itr)->timestamp();

    // TODO(acolwell): Change |frame_end_timestamp| value to
    // |presentation_timestamp + (*itr)->duration()|, like the spec
    // requires, once frame durations are actually present in all buffers.
    TimeDelta frame_end_timestamp = presentation_timestamp;
    if (presentation_timestamp < append_window_start_ ||
        frame_end_timestamp > append_window_end_) {
      DVLOG(1) << "Dropping buffer outside append window."
               << " presentation_timestamp "
               << presentation_timestamp.InSecondsF();
      *needs_keyframe = true;

      // This triggers a discontinuity so we need to treat the next frames
      // appended within the append window as if they were the beginning of a
      // new segment.
      new_media_segment_ = true;
      continue;
    }

    // If |*needs_keyframe| is true then filter out buffers until we
    // encounter the next keyframe.
    if (*needs_keyframe) {
      if (!(*itr)->IsKeyframe()) {
        DVLOG(1) << "Dropping non-keyframe. presentation_timestamp "
                 << presentation_timestamp.InSecondsF();
        continue;
      }

      *needs_keyframe = false;
    }

    filtered_buffers->push_back(*itr);
  }
}

ChunkDemuxerStream::ChunkDemuxerStream(Type type)
    : type_(type),
      state_(UNINITIALIZED) {
}

void ChunkDemuxerStream::StartReturningData() {
  DVLOG(1) << "ChunkDemuxerStream::StartReturningData()";
  base::AutoLock auto_lock(lock_);
  DCHECK(read_cb_.is_null());
  ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

void ChunkDemuxerStream::AbortReads() {
  DVLOG(1) << "ChunkDemuxerStream::AbortReads()";
  base::AutoLock auto_lock(lock_);
  ChangeState_Locked(RETURNING_ABORT_FOR_READS);
  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
}

void ChunkDemuxerStream::CompletePendingReadIfPossible() {
  base::AutoLock auto_lock(lock_);
  if (read_cb_.is_null())
    return;

  CompletePendingReadIfPossible_Locked();
}

void ChunkDemuxerStream::Shutdown() {
  DVLOG(1) << "ChunkDemuxerStream::Shutdown()";
  base::AutoLock auto_lock(lock_);
  ChangeState_Locked(SHUTDOWN);

  // Pass an end of stream buffer to the pending callback to signal that no more
  // data will be sent.
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kOk,
                                        StreamParserBuffer::CreateEOSBuffer());
  }
}

bool ChunkDemuxerStream::IsSeekWaitingForData() const {
  base::AutoLock auto_lock(lock_);
  return stream_->IsSeekPending();
}

void ChunkDemuxerStream::Seek(TimeDelta time) {
  DVLOG(1) << "ChunkDemuxerStream::Seek(" << time.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK(read_cb_.is_null());
  DCHECK(state_ == UNINITIALIZED || state_ == RETURNING_ABORT_FOR_READS)
      << state_;

  stream_->Seek(time);
}

bool ChunkDemuxerStream::Append(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return false;

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, SHUTDOWN);
  if (!stream_->Append(buffers)) {
    DVLOG(1) << "ChunkDemuxerStream::Append() : stream append failed";
    return false;
  }

  if (!read_cb_.is_null())
    CompletePendingReadIfPossible_Locked();

  return true;
}

void ChunkDemuxerStream::Remove(TimeDelta start, TimeDelta end,
                                TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->Remove(start, end, duration);
}

void ChunkDemuxerStream::OnSetDuration(TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->OnSetDuration(duration);
}

Ranges<TimeDelta> ChunkDemuxerStream::GetBufferedRanges(
    TimeDelta duration) const {
  base::AutoLock auto_lock(lock_);
  Ranges<TimeDelta> range = stream_->GetBufferedTime();

  if (range.size() == 0u)
    return range;

  // Clamp the end of the stream's buffered ranges to fit within the duration.
  // This can be done by intersecting the stream's range with the valid time
  // range.
  Ranges<TimeDelta> valid_time_range;
  valid_time_range.Add(range.start(0), duration);
  return range.IntersectionWith(valid_time_range);
}

void ChunkDemuxerStream::OnNewMediaSegment(TimeDelta start_timestamp) {
  DVLOG(2) << "ChunkDemuxerStream::OnNewMediaSegment("
           << start_timestamp.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);
  stream_->OnNewMediaSegment(start_timestamp);
}

bool ChunkDemuxerStream::UpdateAudioConfig(const AudioDecoderConfig& config,
                                           const LogCB& log_cb) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  if (!stream_) {
    DCHECK_EQ(state_, UNINITIALIZED);
    stream_.reset(new SourceBufferStream(config, log_cb));
    return true;
  }

  return stream_->UpdateAudioConfig(config);
}

bool ChunkDemuxerStream::UpdateVideoConfig(const VideoDecoderConfig& config,
                                           const LogCB& log_cb) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);

  if (!stream_) {
    DCHECK_EQ(state_, UNINITIALIZED);
    stream_.reset(new SourceBufferStream(config, log_cb));
    return true;
  }

  return stream_->UpdateVideoConfig(config);
}

void ChunkDemuxerStream::UpdateTextConfig(const TextTrackConfig& config,
                                          const LogCB& log_cb) {
  DCHECK_EQ(type_, TEXT);
  base::AutoLock auto_lock(lock_);
  DCHECK(!stream_);
  DCHECK_EQ(state_, UNINITIALIZED);
  stream_.reset(new SourceBufferStream(config, log_cb));
}

void ChunkDemuxerStream::MarkEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->MarkEndOfStream();
}

void ChunkDemuxerStream::UnmarkEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->UnmarkEndOfStream();
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCB& read_cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, UNINITIALIZED);
  DCHECK(read_cb_.is_null());

  read_cb_ = BindToCurrentLoop(read_cb);
  CompletePendingReadIfPossible_Locked();
}

DemuxerStream::Type ChunkDemuxerStream::type() { return type_; }

void ChunkDemuxerStream::EnableBitstreamConverter() {}

AudioDecoderConfig ChunkDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentAudioDecoderConfig();
}

VideoDecoderConfig ChunkDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentVideoDecoderConfig();
}

TextTrackConfig ChunkDemuxerStream::text_track_config() {
  CHECK_EQ(type_, TEXT);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentTextTrackConfig();
}

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxerStream::ChangeState_Locked() : "
           << "type " << type_
           << " - " << state_ << " -> " << state;
  state_ = state;
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::CompletePendingReadIfPossible_Locked() {
  lock_.AssertAcquired();
  DCHECK(!read_cb_.is_null());

  DemuxerStream::Status status;
  scoped_refptr<StreamParserBuffer> buffer;

  switch (state_) {
    case UNINITIALIZED:
      NOTREACHED();
      return;
    case RETURNING_DATA_FOR_READS:
      switch (stream_->GetNextBuffer(&buffer)) {
        case SourceBufferStream::kSuccess:
          status = DemuxerStream::kOk;
          break;
        case SourceBufferStream::kNeedBuffer:
          // Return early without calling |read_cb_| since we don't have
          // any data to return yet.
          return;
        case SourceBufferStream::kEndOfStream:
          status = DemuxerStream::kOk;
          buffer = StreamParserBuffer::CreateEOSBuffer();
          break;
        case SourceBufferStream::kConfigChange:
          DVLOG(2) << "Config change reported to ChunkDemuxerStream.";
          status = kConfigChanged;
          buffer = NULL;
          break;
      }
      break;
    case RETURNING_ABORT_FOR_READS:
      // Null buffers should be returned in this state since we are waiting
      // for a seek. Any buffers in the SourceBuffer should NOT be returned
      // because they are associated with the seek.
      status = DemuxerStream::kAborted;
      buffer = NULL;
      break;
    case SHUTDOWN:
      status = DemuxerStream::kOk;
      buffer = StreamParserBuffer::CreateEOSBuffer();
      break;
  }

  base::ResetAndReturn(&read_cb_).Run(status, buffer);
}

ChunkDemuxer::ChunkDemuxer(const base::Closure& open_cb,
                           const NeedKeyCB& need_key_cb,
                           const LogCB& log_cb)
    : state_(WAITING_FOR_INIT),
      cancel_next_seek_(false),
      host_(NULL),
      open_cb_(open_cb),
      need_key_cb_(need_key_cb),
      enable_text_(false),
      log_cb_(log_cb),
      duration_(kNoTimestamp()),
      user_specified_duration_(-1) {
  DCHECK(!open_cb_.is_null());
  DCHECK(!need_key_cb_.is_null());
}

void ChunkDemuxer::Initialize(
    DemuxerHost* host,
    const PipelineStatusCB& cb,
    bool enable_text_tracks) {
  DVLOG(1) << "Init()";

  base::AutoLock auto_lock(lock_);

  init_cb_ = BindToCurrentLoop(cb);
  if (state_ == SHUTDOWN) {
    base::ResetAndReturn(&init_cb_).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }
  DCHECK_EQ(state_, WAITING_FOR_INIT);
  host_ = host;
  enable_text_ = enable_text_tracks;

  ChangeState_Locked(INITIALIZING);

  base::ResetAndReturn(&open_cb_).Run();
}

void ChunkDemuxer::Stop(const base::Closure& callback) {
  DVLOG(1) << "Stop()";
  Shutdown();
  callback.Run();
}

void ChunkDemuxer::Seek(TimeDelta time, const PipelineStatusCB& cb) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ")";
  DCHECK(time >= TimeDelta());

  base::AutoLock auto_lock(lock_);
  DCHECK(seek_cb_.is_null());

  seek_cb_ = BindToCurrentLoop(cb);
  if (state_ != INITIALIZED && state_ != ENDED) {
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  if (cancel_next_seek_) {
    cancel_next_seek_ = false;
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
    return;
  }

  SeekAllSources(time);
  StartReturningData();

  if (IsSeekWaitingForData_Locked()) {
    DVLOG(1) << "Seek() : waiting for more data to arrive.";
    return;
  }

  base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

void ChunkDemuxer::OnAudioRendererDisabled() {
  base::AutoLock auto_lock(lock_);
  audio_->Shutdown();
  disabled_audio_ = audio_.Pass();
}

// Demuxer implementation.
DemuxerStream* ChunkDemuxer::GetStream(DemuxerStream::Type type) {
  DCHECK_NE(type, DemuxerStream::TEXT);
  base::AutoLock auto_lock(lock_);
  if (type == DemuxerStream::VIDEO)
    return video_.get();

  if (type == DemuxerStream::AUDIO)
    return audio_.get();

  return NULL;
}

TimeDelta ChunkDemuxer::GetStartTime() const {
  return TimeDelta();
}

void ChunkDemuxer::StartWaitingForSeek(TimeDelta seek_time) {
  DVLOG(1) << "StartWaitingForSeek()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN ||
         state_ == PARSE_ERROR) << state_;
  DCHECK(seek_cb_.is_null());

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  AbortPendingReads();
  SeekAllSources(seek_time);

  // Cancel state set in CancelPendingSeek() since we want to
  // accept the next Seek().
  cancel_next_seek_ = false;
}

void ChunkDemuxer::CancelPendingSeek(TimeDelta seek_time) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, INITIALIZING);
  DCHECK(seek_cb_.is_null() || IsSeekWaitingForData_Locked());

  if (cancel_next_seek_)
    return;

  AbortPendingReads();
  SeekAllSources(seek_time);

  if (seek_cb_.is_null()) {
    cancel_next_seek_ = true;
    return;
  }

  base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  base::AutoLock auto_lock(lock_);

  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) || IsValidId(id))
    return kReachedIdLimit;

  bool has_audio = false;
  bool has_video = false;
  scoped_ptr<media::StreamParser> stream_parser(
      StreamParserFactory::Create(type, codecs, log_cb_,
                                  &has_audio, &has_video));

  if (!stream_parser)
    return ChunkDemuxer::kNotSupported;

  if ((has_audio && !source_id_audio_.empty()) ||
      (has_video && !source_id_video_.empty()))
    return kReachedIdLimit;

  if (has_audio)
    source_id_audio_ = id;

  if (has_video)
    source_id_video_ = id;

  scoped_ptr<SourceState> source_state(
      new SourceState(stream_parser.Pass(), log_cb_,
                      base::Bind(&ChunkDemuxer::CreateDemuxerStream,
                                 base::Unretained(this)),
                      base::Bind(&ChunkDemuxer::IncreaseDurationIfNecessary,
                                 base::Unretained(this))));

  SourceState::NewTextTrackCB new_text_track_cb;

  if (enable_text_) {
    new_text_track_cb = base::Bind(&ChunkDemuxer::OnNewTextTrack,
                                   base::Unretained(this));
  }

  source_state->Init(
      base::Bind(&ChunkDemuxer::OnSourceInitDone, base::Unretained(this)),
      has_audio,
      has_video,
      need_key_cb_,
      new_text_track_cb);

  source_state_map_[id] = source_state.release();
  return kOk;
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId(id));

  delete source_state_map_[id];
  source_state_map_.erase(id);

  if (source_id_audio_ == id)
    source_id_audio_.clear();

  if (source_id_video_ == id)
    source_id_video_.clear();
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges(const std::string& id) const {
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());
  DCHECK(IsValidId(id));
  DCHECK(id == source_id_audio_ || id == source_id_video_);

  if (id == source_id_audio_ && id != source_id_video_) {
    // Only include ranges that have been buffered in |audio_|
    return audio_ ? audio_->GetBufferedRanges(duration_) : Ranges<TimeDelta>();
  }

  if (id != source_id_audio_ && id == source_id_video_) {
    // Only include ranges that have been buffered in |video_|
    return video_ ? video_->GetBufferedRanges(duration_) : Ranges<TimeDelta>();
  }

  return ComputeIntersection();
}

Ranges<TimeDelta> ChunkDemuxer::ComputeIntersection() const {
  lock_.AssertAcquired();

  if (!audio_ || !video_)
    return Ranges<TimeDelta>();

  // Include ranges that have been buffered in both |audio_| and |video_|.
  Ranges<TimeDelta> audio_ranges = audio_->GetBufferedRanges(duration_);
  Ranges<TimeDelta> video_ranges = video_->GetBufferedRanges(duration_);
  Ranges<TimeDelta> result = audio_ranges.IntersectionWith(video_ranges);

  if (state_ == ENDED && result.size() > 0) {
    // If appending has ended, extend the last intersection range to include the
    // max end time of the last audio/video range. This allows the buffered
    // information to match the actual time range that will get played out if
    // the streams have slightly different lengths.
    TimeDelta audio_start = audio_ranges.start(audio_ranges.size() - 1);
    TimeDelta audio_end = audio_ranges.end(audio_ranges.size() - 1);
    TimeDelta video_start = video_ranges.start(video_ranges.size() - 1);
    TimeDelta video_end = video_ranges.end(video_ranges.size() - 1);

    // Verify the last audio range overlaps with the last video range.
    // This is enforced by the logic that controls the transition to ENDED.
    DCHECK((audio_start <= video_start && video_start <= audio_end) ||
           (video_start <= audio_start && audio_start <= video_end));
    result.Add(result.end(result.size()-1), std::max(audio_end, video_end));
  }

  return result;
}

void ChunkDemuxer::AppendData(const std::string& id,
                              const uint8* data,
                              size_t length) {
  DVLOG(1) << "AppendData(" << id << ", " << length << ")";

  DCHECK(!id.empty());

  Ranges<TimeDelta> ranges;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, ENDED);

    // Capture if any of the SourceBuffers are waiting for data before we start
    // parsing.
    bool old_waiting_for_data = IsSeekWaitingForData_Locked();

    if (length == 0u)
      return;

    DCHECK(data);

    switch (state_) {
      case INITIALIZING:
        DCHECK(IsValidId(id));
        if (!source_state_map_[id]->Append(data, length)) {
          ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
          return;
        }
        break;

      case INITIALIZED: {
        DCHECK(IsValidId(id));
        if (!source_state_map_[id]->Append(data, length)) {
          ReportError_Locked(PIPELINE_ERROR_DECODE);
          return;
        }
      } break;

      case PARSE_ERROR:
        DVLOG(1) << "AppendData(): Ignoring data after a parse error.";
        return;

      case WAITING_FOR_INIT:
      case ENDED:
      case SHUTDOWN:
        DVLOG(1) << "AppendData(): called in unexpected state " << state_;
        return;
    }

    // Check to see if data was appended at the pending seek point. This
    // indicates we have parsed enough data to complete the seek.
    if (old_waiting_for_data && !IsSeekWaitingForData_Locked() &&
        !seek_cb_.is_null()) {
      base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
    }

    ranges = GetBufferedRanges_Locked();
  }

  for (size_t i = 0; i < ranges.size(); ++i)
    host_->AddBufferedTimeRange(ranges.start(i), ranges.end(i));
}

void ChunkDemuxer::Abort(const std::string& id) {
  DVLOG(1) << "Abort(" << id << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());
  CHECK(IsValidId(id));
  source_state_map_[id]->Abort();
}

void ChunkDemuxer::Remove(const std::string& id, base::TimeDelta start,
                          base::TimeDelta end) {
  DVLOG(1) << "Remove(" << id << ", " << start.InSecondsF()
           << ", " << end.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);

  if (id == source_id_audio_ && audio_)
    audio_->Remove(start, end, duration_);

  if (id == source_id_video_ && video_)
    video_->Remove(start, end, duration_);
}

double ChunkDemuxer::GetDuration() {
  base::AutoLock auto_lock(lock_);
  return GetDuration_Locked();
}

double ChunkDemuxer::GetDuration_Locked() {
  lock_.AssertAcquired();
  if (duration_ == kNoTimestamp())
    return std::numeric_limits<double>::quiet_NaN();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration_ == kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  if (user_specified_duration_ >= 0)
    return user_specified_duration_;

  return duration_.InSecondsF();
}

void ChunkDemuxer::SetDuration(double duration) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetDuration(" << duration << ")";
  DCHECK_GE(duration, 0);

  if (duration == GetDuration_Locked())
    return;

  // Compute & bounds check the TimeDelta representation of duration.
  // This can be different if the value of |duration| doesn't fit the range or
  // precision of TimeDelta.
  TimeDelta min_duration = TimeDelta::FromInternalValue(1);
  TimeDelta max_duration = TimeDelta::FromInternalValue(kint64max - 1);
  double min_duration_in_seconds = min_duration.InSecondsF();
  double max_duration_in_seconds = max_duration.InSecondsF();

  TimeDelta duration_td;
  if (duration == std::numeric_limits<double>::infinity()) {
    duration_td = media::kInfiniteDuration();
  } else if (duration < min_duration_in_seconds) {
    duration_td = min_duration;
  } else if (duration > max_duration_in_seconds) {
    duration_td = max_duration;
  } else {
    duration_td = TimeDelta::FromMicroseconds(
        duration * base::Time::kMicrosecondsPerSecond);
  }

  DCHECK(duration_td > TimeDelta());

  user_specified_duration_ = duration;
  duration_ = duration_td;
  host_->SetDuration(duration_);

  if (audio_)
    audio_->OnSetDuration(duration_);

  if (video_)
    video_->OnSetDuration(duration_);
}

bool ChunkDemuxer::SetTimestampOffset(const std::string& id, TimeDelta offset) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetTimestampOffset(" << id << ", " << offset.InSecondsF() << ")";
  CHECK(IsValidId(id));

  return source_state_map_[id]->SetTimestampOffset(offset);
}

void ChunkDemuxer::MarkEndOfStream(PipelineStatus status) {
  DVLOG(1) << "MarkEndOfStream(" << status << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, WAITING_FOR_INIT);
  DCHECK_NE(state_, ENDED);

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  if (state_ == INITIALIZING) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  bool old_waiting_for_data = IsSeekWaitingForData_Locked();
  if (audio_)
    audio_->MarkEndOfStream();

  if (video_)
    video_->MarkEndOfStream();

  CompletePendingReadsIfPossible();

  // Give a chance to resume the pending seek process.
  if (status != PIPELINE_OK) {
    ReportError_Locked(status);
    return;
  }

  ChangeState_Locked(ENDED);
  DecreaseDurationIfNecessary();

  if (old_waiting_for_data && !IsSeekWaitingForData_Locked() &&
      !seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
  }
}

void ChunkDemuxer::UnmarkEndOfStream() {
  DVLOG(1) << "UnmarkEndOfStream()";
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, ENDED);

  ChangeState_Locked(INITIALIZED);

  if (audio_)
    audio_->UnmarkEndOfStream();

  if (video_)
    video_->UnmarkEndOfStream();
}

void ChunkDemuxer::SetAppendWindowStart(const std::string& id,
                                        TimeDelta start) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetAppendWindowStart(" << id << ", "
           << start.InSecondsF() << ")";
  CHECK(IsValidId(id));
  source_state_map_[id]->set_append_window_start(start);
}

void ChunkDemuxer::SetAppendWindowEnd(const std::string& id, TimeDelta end) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetAppendWindowEnd(" << id << ", " << end.InSecondsF() << ")";
  CHECK(IsValidId(id));
  source_state_map_[id]->set_append_window_end(end);
}

void ChunkDemuxer::Shutdown() {
  DVLOG(1) << "Shutdown()";
  base::AutoLock auto_lock(lock_);

  if (state_ == SHUTDOWN)
    return;

  if (audio_)
    audio_->Shutdown();

  if (video_)
    video_->Shutdown();

  ChangeState_Locked(SHUTDOWN);

  if(!seek_cb_.is_null())
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_ERROR_ABORT);
}

void ChunkDemuxer::SetMemoryLimitsForTesting(int memory_limit) {
  if (audio_)
    audio_->set_memory_limit_for_testing(memory_limit);

  if (video_)
    video_->set_memory_limit_for_testing(memory_limit);
}

void ChunkDemuxer::ChangeState_Locked(State new_state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxer::ChangeState_Locked() : "
           << state_ << " -> " << new_state;
  state_ = new_state;
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);
  for (SourceStateMap::iterator it = source_state_map_.begin();
       it != source_state_map_.end(); ++it) {
    delete it->second;
  }
  source_state_map_.clear();
}

void ChunkDemuxer::ReportError_Locked(PipelineStatus error) {
  DVLOG(1) << "ReportError_Locked(" << error << ")";
  lock_.AssertAcquired();
  DCHECK_NE(error, PIPELINE_OK);

  ChangeState_Locked(PARSE_ERROR);

  PipelineStatusCB cb;

  if (!init_cb_.is_null()) {
    std::swap(cb, init_cb_);
  } else {
    if (!seek_cb_.is_null())
      std::swap(cb, seek_cb_);

    if (audio_)
      audio_->Shutdown();

    if (video_)
      video_->Shutdown();
  }

  if (!cb.is_null()) {
    cb.Run(error);
    return;
  }

  base::AutoUnlock auto_unlock(lock_);
  host_->OnDemuxerError(error);
}

bool ChunkDemuxer::IsSeekWaitingForData_Locked() const {
  lock_.AssertAcquired();
  bool waiting_for_data = false;

  if (audio_)
    waiting_for_data = audio_->IsSeekWaitingForData();

  if (!waiting_for_data && video_)
    waiting_for_data = video_->IsSeekWaitingForData();

  return waiting_for_data;
}

void ChunkDemuxer::OnSourceInitDone(bool success, TimeDelta duration) {
  DVLOG(1) << "OnSourceInitDone(" << success << ", "
           << duration.InSecondsF() << ")";
  lock_.AssertAcquired();
  DCHECK_EQ(state_, INITIALIZING);
  if (!success || (!audio_ && !video_)) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (duration != TimeDelta() && duration_ == kNoTimestamp())
    UpdateDuration(duration);

  // Wait until all streams have initialized.
  if ((!source_id_audio_.empty() && !audio_) ||
      (!source_id_video_.empty() && !video_))
    return;

  SeekAllSources(GetStartTime());
  StartReturningData();

  if (duration_ == kNoTimestamp())
    duration_ = kInfiniteDuration();

  // The demuxer is now initialized after the |start_timestamp_| was set.
  ChangeState_Locked(INITIALIZED);
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

ChunkDemuxerStream*
ChunkDemuxer::CreateDemuxerStream(DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      if (audio_)
        return NULL;
      audio_.reset(new ChunkDemuxerStream(DemuxerStream::AUDIO));
      return audio_.get();
      break;
    case DemuxerStream::VIDEO:
      if (video_)
        return NULL;
      video_.reset(new ChunkDemuxerStream(DemuxerStream::VIDEO));
      return video_.get();
      break;
    case DemuxerStream::TEXT: {
      return new ChunkDemuxerStream(DemuxerStream::TEXT);
      break;
    }
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::NUM_TYPES:
      NOTREACHED();
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

void ChunkDemuxer::OnNewTextTrack(ChunkDemuxerStream* text_stream,
                                  const TextTrackConfig& config) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);
  host_->AddTextStream(text_stream, config);
}

bool ChunkDemuxer::IsValidId(const std::string& source_id) const {
  lock_.AssertAcquired();
  return source_state_map_.count(source_id) > 0u;
}

void ChunkDemuxer::UpdateDuration(TimeDelta new_duration) {
  DCHECK(duration_ != new_duration);
  user_specified_duration_ = -1;
  duration_ = new_duration;
  host_->SetDuration(new_duration);
}

void ChunkDemuxer::IncreaseDurationIfNecessary(
    TimeDelta last_appended_buffer_timestamp,
    ChunkDemuxerStream* stream) {
  DCHECK(last_appended_buffer_timestamp != kNoTimestamp());
  if (last_appended_buffer_timestamp <= duration_)
    return;

  Ranges<TimeDelta> ranges = stream->GetBufferedRanges(kInfiniteDuration());
  DCHECK_GT(ranges.size(), 0u);

  TimeDelta last_timestamp_buffered = ranges.end(ranges.size() - 1);
  if (last_timestamp_buffered > duration_)
    UpdateDuration(last_timestamp_buffered);
}

void ChunkDemuxer::DecreaseDurationIfNecessary() {
  lock_.AssertAcquired();
  Ranges<TimeDelta> ranges = GetBufferedRanges_Locked();
  if (ranges.size() == 0u)
    return;

  TimeDelta last_timestamp_buffered = ranges.end(ranges.size() - 1);
  if (last_timestamp_buffered < duration_)
    UpdateDuration(last_timestamp_buffered);
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges() const {
  base::AutoLock auto_lock(lock_);
  return GetBufferedRanges_Locked();
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges_Locked() const {
  lock_.AssertAcquired();
  if (audio_ && !video_)
    return audio_->GetBufferedRanges(duration_);
  else if (!audio_ && video_)
    return video_->GetBufferedRanges(duration_);
  return ComputeIntersection();
}

void ChunkDemuxer::StartReturningData() {
  for (SourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->StartReturningData();
  }
}

void ChunkDemuxer::AbortPendingReads() {
  for (SourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->AbortReads();
  }
}

void ChunkDemuxer::SeekAllSources(TimeDelta seek_time) {
  for (SourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->Seek(seek_time);
  }
}

void ChunkDemuxer::CompletePendingReadsIfPossible() {
  for (SourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->CompletePendingReadIfPossible();
  }
}

}  // namespace media
