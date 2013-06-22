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
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/stream_parser_factory.h"
#include "media/webm/webm_webvtt_parser.h"

using base::TimeDelta;

namespace media {

// Contains state belonging to a source id.
class SourceState {
 public:
  explicit SourceState(scoped_ptr<StreamParser> stream_parser);

  void Init(const StreamParser::InitCB& init_cb,
            const StreamParser::NewConfigCB& config_cb,
            const StreamParser::NewBuffersCB& audio_cb,
            const StreamParser::NewBuffersCB& video_cb,
            const StreamParser::NewTextBuffersCB& text_cb,
            const StreamParser::NeedKeyCB& need_key_cb,
            const AddTextTrackCB& add_text_track_cb,
            const StreamParser::NewMediaSegmentCB& new_segment_cb,
            const LogCB& log_cb);

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

 private:
  // Called by the |stream_parser_| at the beginning of a new media segment.
  // |timestamp| is the timestamp on the first buffer in the segment.
  // It modifies the state of this object and then calls |new_segment_cb| with
  // modified version of |timestamp|.
  void OnNewMediaSegment(const StreamParser::NewMediaSegmentCB& new_segment_cb,
                         TimeDelta timestamp);

  // Called by the |stream_parser_| at the end of a media segment.
  void OnEndOfMediaSegment();

  // Called by the |stream_parser_| when new buffers have been parsed. It
  // applies |timestamp_offset_| to all buffers in |buffers| and then calls
  // |new_buffers_cb| with the modified buffers.
  // Returns true on a successful call. Returns false if an error occured while
  // processing the buffers.
  bool OnBuffers(const StreamParser::NewBuffersCB& new_buffers_cb,
                 const StreamParser::BufferQueue& buffers);

  // Called by the |stream_parser_| when new text buffers have been parsed. It
  // applies |timestamp_offset_| to all buffers in |buffers| and then calls
  // |new_buffers_cb| with the modified buffers.
  // Returns true on a successful call. Returns false if an error occured while
  // processing the buffers.
  bool OnTextBuffers(const StreamParser::NewTextBuffersCB& new_buffers_cb,
                     TextTrack* text_track,
                     const StreamParser::BufferQueue& buffers);

  // Helper function that adds |timestamp_offset_| to each buffer in |buffers|.
  void AdjustBufferTimestamps(const StreamParser::BufferQueue& buffers);

  // The offset to apply to media segment timestamps.
  TimeDelta timestamp_offset_;

  // Keeps track of whether |timestamp_offset_| can be modified.
  bool can_update_offset_;

  // The object used to parse appended data.
  scoped_ptr<StreamParser> stream_parser_;

  DISALLOW_COPY_AND_ASSIGN(SourceState);
};

SourceState::SourceState(scoped_ptr<StreamParser> stream_parser)
    : can_update_offset_(true),
      stream_parser_(stream_parser.release()) {
}

void SourceState::Init(const StreamParser::InitCB& init_cb,
                       const StreamParser::NewConfigCB& config_cb,
                       const StreamParser::NewBuffersCB& audio_cb,
                       const StreamParser::NewBuffersCB& video_cb,
                       const StreamParser::NewTextBuffersCB& text_cb,
                       const StreamParser::NeedKeyCB& need_key_cb,
                       const AddTextTrackCB& add_text_track_cb,
                       const StreamParser::NewMediaSegmentCB& new_segment_cb,
                       const LogCB& log_cb) {
  stream_parser_->Init(init_cb, config_cb,
                       base::Bind(&SourceState::OnBuffers,
                                  base::Unretained(this), audio_cb),
                       base::Bind(&SourceState::OnBuffers,
                                  base::Unretained(this), video_cb),
                       base::Bind(&SourceState::OnTextBuffers,
                                  base::Unretained(this), text_cb),
                       need_key_cb,
                       add_text_track_cb,
                       base::Bind(&SourceState::OnNewMediaSegment,
                                  base::Unretained(this), new_segment_cb),
                       base::Bind(&SourceState::OnEndOfMediaSegment,
                                  base::Unretained(this)),
                       log_cb);
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
  can_update_offset_ = true;
}

void SourceState::AdjustBufferTimestamps(
    const StreamParser::BufferQueue& buffers) {
  if (timestamp_offset_ == TimeDelta())
    return;

  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetDecodeTimestamp(
        (*itr)->GetDecodeTimestamp() + timestamp_offset_);
    (*itr)->SetTimestamp((*itr)->GetTimestamp() + timestamp_offset_);
  }
}

void SourceState::OnNewMediaSegment(
    const StreamParser::NewMediaSegmentCB& new_segment_cb,
    TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp());
  DVLOG(2) << "OnNewMediaSegment(" << timestamp.InSecondsF() << ")";

  can_update_offset_ = false;
  new_segment_cb.Run(timestamp + timestamp_offset_);
}

void SourceState::OnEndOfMediaSegment() {
  DVLOG(2) << "OnEndOfMediaSegment()";
  can_update_offset_ = true;
}

bool SourceState::OnBuffers(const StreamParser::NewBuffersCB& new_buffers_cb,
                            const StreamParser::BufferQueue& buffers) {
  if (new_buffers_cb.is_null())
    return false;

  AdjustBufferTimestamps(buffers);

  return new_buffers_cb.Run(buffers);
}

bool SourceState::OnTextBuffers(
    const StreamParser::NewTextBuffersCB& new_buffers_cb,
    TextTrack* text_track,
    const StreamParser::BufferQueue& buffers) {
  if (new_buffers_cb.is_null())
    return false;

  AdjustBufferTimestamps(buffers);

  return new_buffers_cb.Run(text_track, buffers);
}

class ChunkDemuxerStream : public DemuxerStream {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;
  typedef std::deque<ReadCB> ReadCBQueue;
  typedef std::deque<base::Closure> ClosureQueue;

  ChunkDemuxerStream(const AudioDecoderConfig& audio_config,
                     const LogCB& log_cb);
  ChunkDemuxerStream(const VideoDecoderConfig& video_config,
                     const LogCB& log_cb);
  virtual ~ChunkDemuxerStream();

  void AbortReads();
  void Seek(TimeDelta time);
  bool IsSeekWaitingForData() const;

  // Add buffers to this stream.  Buffers are stored in SourceBufferStreams,
  // which handle ordering and overlap resolution.
  // Returns true if buffers were successfully added.
  bool Append(const StreamParser::BufferQueue& buffers);

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
  bool UpdateAudioConfig(const AudioDecoderConfig& config);
  bool UpdateVideoConfig(const VideoDecoderConfig& config);

  void EndOfStream();
  void CancelEndOfStream();

  void Shutdown();

  // DemuxerStream methods.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;
  virtual AudioDecoderConfig audio_decoder_config() OVERRIDE;
  virtual VideoDecoderConfig video_decoder_config() OVERRIDE;

 private:
  enum State {
    UNINITIALIZED,
    RETURNING_DATA_FOR_READS,
    RETURNING_ABORT_FOR_READS,
    SHUTDOWN,
  };

  // Assigns |state_| to |state|
  void ChangeState_Locked(State state);

  // Adds the callback to |read_cbs_| so it can be called later when we
  // have data.
  void DeferRead_Locked(const ReadCB& read_cb);

  // Creates closures that bind ReadCBs in |read_cbs_| to data in
  // |buffers_| and pops the callbacks & buffers from the respective queues.
  void CreateReadDoneClosures_Locked(ClosureQueue* closures);

  // Gets the value to pass to the next Read() callback. Returns true if
  // |status| and |buffer| should be passed to the callback. False indicates
  // that |status| and |buffer| were not set and more data is needed.
  bool GetNextBuffer_Locked(DemuxerStream::Status* status,
                            scoped_refptr<StreamParserBuffer>* buffer);

  // Specifies the type of the stream (must be AUDIO or VIDEO for now).
  Type type_;

  scoped_ptr<SourceBufferStream> stream_;

  mutable base::Lock lock_;
  State state_;
  ReadCBQueue read_cbs_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

ChunkDemuxerStream::ChunkDemuxerStream(const AudioDecoderConfig& audio_config,
                                       const LogCB& log_cb)
    : type_(AUDIO),
      state_(UNINITIALIZED) {
  stream_.reset(new SourceBufferStream(audio_config, log_cb));
}

ChunkDemuxerStream::ChunkDemuxerStream(const VideoDecoderConfig& video_config,
                                       const LogCB& log_cb)
    : type_(VIDEO),
      state_(UNINITIALIZED) {
  stream_.reset(new SourceBufferStream(video_config, log_cb));
}

void ChunkDemuxerStream::AbortReads() {
  DVLOG(1) << "ChunkDemuxerStream::AbortReads()";
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(RETURNING_ABORT_FOR_READS);
    std::swap(read_cbs_, read_cbs);
  }

  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(kAborted, NULL);
}

void ChunkDemuxerStream::Seek(TimeDelta time) {
  base::AutoLock auto_lock(lock_);
  DCHECK(read_cbs_.empty());
  DCHECK(state_ == UNINITIALIZED || state_ == RETURNING_ABORT_FOR_READS);

  stream_->Seek(time);
  ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

bool ChunkDemuxerStream::IsSeekWaitingForData() const {
  base::AutoLock auto_lock(lock_);
  return stream_->IsSeekPending();
}

void ChunkDemuxerStream::OnNewMediaSegment(TimeDelta start_timestamp) {
  base::AutoLock auto_lock(lock_);
  stream_->OnNewMediaSegment(start_timestamp);
}

bool ChunkDemuxerStream::Append(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return false;

  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, SHUTDOWN);
    if (!stream_->Append(buffers)) {
      DVLOG(1) << "ChunkDemuxerStream::Append() : stream append failed";
      return false;
    }
    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();

  return true;
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

bool ChunkDemuxerStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  return stream_->UpdateAudioConfig(config);
}

bool ChunkDemuxerStream::UpdateVideoConfig(const VideoDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  return stream_->UpdateVideoConfig(config);
}

void ChunkDemuxerStream::EndOfStream() {
  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    stream_->EndOfStream();
    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();
}

void ChunkDemuxerStream::CancelEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->CancelEndOfStream();
}

void ChunkDemuxerStream::Shutdown() {
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(SHUTDOWN);
    std::swap(read_cbs_, read_cbs);
  }

  // Pass end of stream buffers to all callbacks to signal that no more data
  // will be sent.
  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(DemuxerStream::kOk, StreamParserBuffer::CreateEOSBuffer());
}

// Helper function that makes sure |read_cb| runs on |message_loop_proxy|.
static void RunOnMessageLoop(
    const DemuxerStream::ReadCB& read_cb,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!message_loop_proxy->BelongsToCurrentThread()) {
    message_loop_proxy->PostTask(FROM_HERE, base::Bind(
        &RunOnMessageLoop, read_cb, message_loop_proxy, status, buffer));
    return;
  }

  read_cb.Run(status, buffer);
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCB& read_cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, UNINITIALIZED);

  DemuxerStream::Status status = kOk;
  scoped_refptr<StreamParserBuffer> buffer;

  if (!read_cbs_.empty() || !GetNextBuffer_Locked(&status, &buffer)) {
    DeferRead_Locked(read_cb);
    return;
  }

  base::MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
      read_cb, status, buffer));
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

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxerStream::ChangeState_Locked() : "
           << "type " << type_
           << " - " << state_ << " -> " << state;
  state_ = state;
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::DeferRead_Locked(const ReadCB& read_cb) {
  lock_.AssertAcquired();
  // Wrap & store |read_cb| so that it will
  // get called on the current MessageLoop.
  read_cbs_.push_back(base::Bind(&RunOnMessageLoop, read_cb,
                                 base::MessageLoopProxy::current()));
}

void ChunkDemuxerStream::CreateReadDoneClosures_Locked(ClosureQueue* closures) {
  lock_.AssertAcquired();

  if (state_ != RETURNING_DATA_FOR_READS)
    return;

  DemuxerStream::Status status;
  scoped_refptr<StreamParserBuffer> buffer;
  while (!read_cbs_.empty()) {
    if (!GetNextBuffer_Locked(&status, &buffer))
      return;
    closures->push_back(base::Bind(read_cbs_.front(),
                                   status, buffer));
    read_cbs_.pop_front();
  }
}

bool ChunkDemuxerStream::GetNextBuffer_Locked(
    DemuxerStream::Status* status,
    scoped_refptr<StreamParserBuffer>* buffer) {
  lock_.AssertAcquired();

  switch (state_) {
    case UNINITIALIZED:
      NOTREACHED();
      return false;
    case RETURNING_DATA_FOR_READS:
      switch (stream_->GetNextBuffer(buffer)) {
        case SourceBufferStream::kSuccess:
          *status = DemuxerStream::kOk;
          return true;
        case SourceBufferStream::kNeedBuffer:
          return false;
        case SourceBufferStream::kEndOfStream:
          *status = DemuxerStream::kOk;
          *buffer = StreamParserBuffer::CreateEOSBuffer();
          return true;
        case SourceBufferStream::kConfigChange:
          DVLOG(2) << "Config change reported to ChunkDemuxerStream.";
          *status = kConfigChanged;
          *buffer = NULL;
          return true;
      }
      break;
    case RETURNING_ABORT_FOR_READS:
      // Null buffers should be returned in this state since we are waiting
      // for a seek. Any buffers in the SourceBuffer should NOT be returned
      // because they are associated with the seek.
      DCHECK(read_cbs_.empty());
      *status = DemuxerStream::kAborted;
      *buffer = NULL;
      return true;
    case SHUTDOWN:
      DCHECK(read_cbs_.empty());
      *status = DemuxerStream::kOk;
      *buffer = StreamParserBuffer::CreateEOSBuffer();
      return true;
  }

  NOTREACHED();
  return false;
}

ChunkDemuxer::ChunkDemuxer(const base::Closure& open_cb,
                           const NeedKeyCB& need_key_cb,
                           const AddTextTrackCB& add_text_track_cb,
                           const LogCB& log_cb)
    : state_(WAITING_FOR_INIT),
      cancel_next_seek_(false),
      host_(NULL),
      open_cb_(open_cb),
      need_key_cb_(need_key_cb),
      add_text_track_cb_(add_text_track_cb),
      log_cb_(log_cb),
      duration_(kNoTimestamp()),
      user_specified_duration_(-1) {
  DCHECK(!open_cb_.is_null());
  DCHECK(!need_key_cb_.is_null());
}

void ChunkDemuxer::Initialize(DemuxerHost* host, const PipelineStatusCB& cb) {
  DVLOG(1) << "Init()";

  base::AutoLock auto_lock(lock_);

  init_cb_ = BindToCurrentLoop(cb);
  if (state_ == SHUTDOWN) {
    base::ResetAndReturn(&init_cb_).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }
  DCHECK_EQ(state_, WAITING_FOR_INIT);
  host_ = host;

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

  if (audio_)
    audio_->Seek(time);

  if (video_)
    video_->Seek(time);

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

void ChunkDemuxer::StartWaitingForSeek() {
  DVLOG(1) << "StartWaitingForSeek()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN);
  DCHECK(seek_cb_.is_null());

  if (state_ == SHUTDOWN)
    return;

  if (audio_)
    audio_->AbortReads();

  if (video_)
    video_->AbortReads();

  // Cancel state set in CancelPendingSeek() since we want to
  // accept the next Seek().
  cancel_next_seek_ = false;
}

void ChunkDemuxer::CancelPendingSeek() {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, INITIALIZING);
  DCHECK(seek_cb_.is_null() || IsSeekWaitingForData_Locked());

  if (cancel_next_seek_)
    return;

  if (audio_)
    audio_->AbortReads();

  if (video_)
    video_->AbortReads();

  if (seek_cb_.is_null()) {
    cancel_next_seek_ = true;
    return;
  }

  base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  DCHECK_GT(codecs.size(), 0u);
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

  StreamParser::NewBuffersCB audio_cb;
  StreamParser::NewBuffersCB video_cb;

  if (has_audio) {
    source_id_audio_ = id;
    audio_cb = base::Bind(&ChunkDemuxer::OnAudioBuffers,
                          base::Unretained(this));
  }

  if (has_video) {
    source_id_video_ = id;
    video_cb = base::Bind(&ChunkDemuxer::OnVideoBuffers,
                          base::Unretained(this));
  }

  scoped_ptr<SourceState> source_state(new SourceState(stream_parser.Pass()));
  source_state->Init(
      base::Bind(&ChunkDemuxer::OnSourceInitDone, base::Unretained(this)),
      base::Bind(&ChunkDemuxer::OnNewConfigs, base::Unretained(this),
                 has_audio, has_video),
      audio_cb,
      video_cb,
      base::Bind(&ChunkDemuxer::OnTextBuffers, base::Unretained(this)),
      base::Bind(&ChunkDemuxer::OnNeedKey, base::Unretained(this)),
      add_text_track_cb_,
      base::Bind(&ChunkDemuxer::OnNewMediaSegment, base::Unretained(this), id),
      log_cb_);

  source_state_map_[id] = source_state.release();
  return kOk;
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId(id));

  delete source_state_map_[id];
  source_state_map_.erase(id);

  if (source_id_audio_ == id) {
    if (audio_)
      audio_->Shutdown();
    source_id_audio_.clear();
  }

  if (source_id_video_ == id) {
    if (video_)
      video_->Shutdown();
    source_id_video_.clear();
  }
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

    // Capture if any of the SourceBuffers are waiting for data before we start
    // parsing.
    bool old_waiting_for_data = IsSeekWaitingForData_Locked();

    if (state_ == ENDED) {
      ChangeState_Locked(INITIALIZED);

      if (audio_)
        audio_->CancelEndOfStream();

      if (video_)
        video_->CancelEndOfStream();
    }

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

    ranges = GetBufferedRanges();
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

void ChunkDemuxer::EndOfStream(PipelineStatus status) {
  DVLOG(1) << "EndOfStream(" << status << ")";
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
    audio_->EndOfStream();

  if (video_)
    video_->EndOfStream();

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

  TimeDelta start_time = GetStartTime();
  if (audio_)
    audio_->Seek(start_time);

  if (video_)
    video_->Seek(start_time);

  if (duration_ == kNoTimestamp())
    duration_ = kInfiniteDuration();

  // The demuxer is now initialized after the |start_timestamp_| was set.
  ChangeState_Locked(INITIALIZED);
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

bool ChunkDemuxer::OnNewConfigs(bool has_audio, bool has_video,
                                const AudioDecoderConfig& audio_config,
                                const VideoDecoderConfig& video_config) {
  DVLOG(1) << "OnNewConfigs(" << has_audio << ", " << has_video
           << ", " << audio_config.IsValidConfig()
           << ", " << video_config.IsValidConfig() << ")";
  lock_.AssertAcquired();

  if (!audio_config.IsValidConfig() && !video_config.IsValidConfig()) {
    DVLOG(1) << "OnNewConfigs() : Audio & video config are not valid!";
    return false;
  }

  // Signal an error if we get configuration info for stream types that weren't
  // specified in AddId() or more configs after a stream is initialized.
  // Only allow a single audio config for now.
  if (has_audio != audio_config.IsValidConfig()) {
    MEDIA_LOG(log_cb_)
        << "Initialization segment"
        << (audio_config.IsValidConfig() ? " has" : " does not have")
        << " an audio track, but the mimetype"
        << (has_audio ? " specifies" : " does not specify")
        << " an audio codec.";
    return false;
  }

  // Only allow a single video config for now.
  if (has_video != video_config.IsValidConfig()) {
    MEDIA_LOG(log_cb_)
        << "Initialization segment"
        << (video_config.IsValidConfig() ? " has" : " does not have")
        << " a video track, but the mimetype"
        << (has_video ? " specifies" : " does not specify")
        << " a video codec.";
    return false;
  }

  bool success = true;
  if (audio_config.IsValidConfig()) {
    if (audio_) {
      success &= audio_->UpdateAudioConfig(audio_config);
    } else {
      audio_.reset(new ChunkDemuxerStream(audio_config, log_cb_));
    }
  }

  if (video_config.IsValidConfig()) {
    if (video_) {
      success &= video_->UpdateVideoConfig(video_config);
    } else {
      video_.reset(new ChunkDemuxerStream(video_config, log_cb_));
    }
  }

  DVLOG(1) << "OnNewConfigs() : " << (success ? "success" : "failed");
  return success;
}

bool ChunkDemuxer::OnAudioBuffers(const StreamParser::BufferQueue& buffers) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);

  if (!audio_)
    return false;

  CHECK(IsValidId(source_id_audio_));
  if (!audio_->Append(buffers))
    return false;

  IncreaseDurationIfNecessary(buffers, audio_.get());
  return true;
}

bool ChunkDemuxer::OnVideoBuffers(const StreamParser::BufferQueue& buffers) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);

  if (!video_)
    return false;

  CHECK(IsValidId(source_id_video_));
  if (!video_->Append(buffers))
    return false;

  IncreaseDurationIfNecessary(buffers, video_.get());
  return true;
}

bool ChunkDemuxer::OnTextBuffers(
  TextTrack* text_track,
  const StreamParser::BufferQueue& buffers) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);

  // TODO(matthewjheaney): IncreaseDurationIfNecessary

  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    const StreamParserBuffer* const buffer = itr->get();
    const TimeDelta start = buffer->GetTimestamp();
    const TimeDelta end = start + buffer->GetDuration();

    std::string id, settings, content;

    WebMWebVTTParser::Parse(buffer->GetData(),
                            buffer->GetDataSize(),
                            &id, &settings, &content);

    text_track->addWebVTTCue(start, end, id, content, settings);
  }

  return true;
}

// TODO(acolwell): Remove bool from StreamParser::NeedKeyCB so that
// this method can be removed and need_key_cb_ can be passed directly
// to the parser.
bool ChunkDemuxer::OnNeedKey(const std::string& type,
                             scoped_ptr<uint8[]> init_data,
                             int init_data_size) {
  lock_.AssertAcquired();
  need_key_cb_.Run(type, init_data.Pass(), init_data_size);
  return true;
}

void ChunkDemuxer::OnNewMediaSegment(const std::string& source_id,
                                     TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp());
  DVLOG(2) << "OnNewMediaSegment(" << source_id << ", "
           << timestamp.InSecondsF() << ")";
  lock_.AssertAcquired();

  CHECK(IsValidId(source_id));
  if (audio_ && source_id == source_id_audio_)
    audio_->OnNewMediaSegment(timestamp);
  if (video_ && source_id == source_id_video_)
    video_->OnNewMediaSegment(timestamp);
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
    const StreamParser::BufferQueue& buffers,
    ChunkDemuxerStream* stream) {
  DCHECK(!buffers.empty());
  if (buffers.back()->GetTimestamp() <= duration_)
    return;

  Ranges<TimeDelta> ranges = stream->GetBufferedRanges(kInfiniteDuration());
  DCHECK_GT(ranges.size(), 0u);

  TimeDelta last_timestamp_buffered = ranges.end(ranges.size() - 1);
  if (last_timestamp_buffered > duration_)
    UpdateDuration(last_timestamp_buffered);
}

void ChunkDemuxer::DecreaseDurationIfNecessary() {
  Ranges<TimeDelta> ranges = GetBufferedRanges();
  if (ranges.size() == 0u)
    return;

  TimeDelta last_timestamp_buffered = ranges.end(ranges.size() - 1);
  if (last_timestamp_buffered < duration_)
    UpdateDuration(last_timestamp_buffered);
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges() const {
  if (audio_ && !video_)
    return audio_->GetBufferedRanges(duration_);
  else if (!audio_ && video_)
    return video_->GetBufferedRanges(duration_);
  return ComputeIntersection();
}

}  // namespace media
