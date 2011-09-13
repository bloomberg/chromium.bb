// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/adaptive_demuxer.h"

namespace media {

static const int64 kSwitchTimerPeriod = 5000;  // In milliseconds.

// Object that decides when to switch streams.
class StreamSwitchManager
    : public base::RefCountedThreadSafe<StreamSwitchManager> {
 public:
  typedef std::vector<int> StreamIdVector;

  StreamSwitchManager(AdaptiveDemuxer* demuxer,
                      const StreamIdVector& video_ids);
  virtual ~StreamSwitchManager();

  // Playback events. These methods are called when playback starts, pauses, or
  // stops.
  void Play();
  void Pause();
  void Stop();

 private:
  // Method called periodically to determine if we should switch
  // streams.
  void OnSwitchTimer();

  // Called when the demuxer completes a stream switch.
  void OnSwitchDone(PipelineStatus status);

  // The demuxer that owns this object.
  AdaptiveDemuxer* demuxer_;

  // Message loop this object runs on.
  MessageLoop* message_loop_;

  // Is clip playing or not?
  bool playing_;

  // Is a stream switch in progress?
  bool switch_pending_;

  // Periodic timer that calls OnSwitchTimer().
  base::RepeatingTimer<StreamSwitchManager> timer_;

  // The stream IDs for the video streams.
  StreamIdVector video_ids_;

  // An index into |video_ids_| for the current video stream.
  int current_id_index_;

  DISALLOW_COPY_AND_ASSIGN(StreamSwitchManager);
};

StreamSwitchManager::StreamSwitchManager(
    AdaptiveDemuxer* demuxer,
    const StreamIdVector& video_ids)
    : demuxer_(demuxer),
      playing_(false),
      switch_pending_(false),
      video_ids_(video_ids),
      current_id_index_(-1) {
  DCHECK(demuxer);
  message_loop_ = MessageLoop::current();
  demuxer_ = demuxer;
  current_id_index_ = -1;

  if (video_ids_.size() > 0) {
    // Find the index in video_ids_ for the current video ID.
    int current_id = demuxer->GetCurrentVideoId();
    current_id_index_ = 0;
    for (size_t i = 0; i < video_ids_.size(); i++) {
      if (current_id == video_ids_[i]) {
        current_id_index_ = i;
        break;
      }
    }
  }
}

StreamSwitchManager::~StreamSwitchManager() {}

void StreamSwitchManager::Play() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!playing_);
  playing_ = true;

  if (video_ids_.size() > 1) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kSwitchTimerPeriod),
                 this, &StreamSwitchManager::OnSwitchTimer);
  }
}

void StreamSwitchManager::Pause() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(playing_);
  playing_ = false;
  timer_.Stop();
}

void StreamSwitchManager::Stop() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!playing_);
  demuxer_ = NULL;
}

void StreamSwitchManager::OnSwitchTimer() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (!playing_ || !demuxer_ || (video_ids_.size() < 2) || switch_pending_)
    return;

  // Select the stream to switch to. For now we are just rotating
  // through the available streams.
  int new_id_index = (current_id_index_ + 1) % video_ids_.size();

  current_id_index_ = new_id_index;
  switch_pending_ = true;
  demuxer_->ChangeVideoStream(video_ids_[new_id_index],
                              base::Bind(&StreamSwitchManager::OnSwitchDone,
                                         this));
}

void StreamSwitchManager::OnSwitchDone(PipelineStatus status) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &StreamSwitchManager::OnSwitchDone, status));
    return;
  }

  switch_pending_ = false;
}

//
// AdaptiveDemuxerStream
//
AdaptiveDemuxerStream::AdaptiveDemuxerStream(
    StreamVector const& streams, int initial_stream)
    : streams_(streams), current_stream_index_(initial_stream),
      bitstream_converter_enabled_(false),
      pending_reads_(0),
      switch_index_(-1),
      last_buffer_timestamp_(kNoTimestamp) {
  DCheckSanity();
}

void AdaptiveDemuxerStream::DCheckSanity() {
  // We only carry out sanity checks in debug mode.
  if (!logging::DEBUG_MODE)
    return;
  DCHECK(streams_[current_stream_index_].get());

  bool non_null_stream_seen = false;
  Type type = DemuxerStream::UNKNOWN;
  for (size_t i = 0; i < streams_.size(); ++i) {
    if (!streams_[i])
      continue;

    if (!non_null_stream_seen) {
      non_null_stream_seen = true;
      type = streams_[i]->type();
    } else {
      DCHECK_EQ(streams_[i]->type(), type);
    }
  }
}

AdaptiveDemuxerStream::~AdaptiveDemuxerStream() {
  base::AutoLock auto_lock(lock_);
  current_stream_index_ = -1;
  streams_.clear();
}

DemuxerStream* AdaptiveDemuxerStream::current_stream() {
  base::AutoLock auto_lock(lock_);
  return streams_[current_stream_index_];
}

void AdaptiveDemuxerStream::Read(const ReadCallback& read_callback) {
  DemuxerStream* stream = NULL;

  {
    base::AutoLock auto_lock(lock_);

    read_cb_queue_.push_back(read_callback);

    // Check to make sure we aren't doing a stream switch. We only want to
    // make calls on |streams_[current_stream_index_]| when we aren't
    // in the middle of a stream switch. Since the callback is stored in
    // |read_cb_queue_| we will issue the Read() on the new stream once
    // the switch has completed.
    if (!IsSwitchPending_Locked()) {
      stream = streams_[current_stream_index_];
      pending_reads_++;
    }
  }

  if (stream)
    stream->Read(base::Bind(&AdaptiveDemuxerStream::OnReadDone, this));
}

AVStream* AdaptiveDemuxerStream::GetAVStream() {
  return current_stream()->GetAVStream();
}

DemuxerStream::Type AdaptiveDemuxerStream::type() {
  return current_stream()->type();
}

void AdaptiveDemuxerStream::EnableBitstreamConverter() {
  {
    base::AutoLock auto_lock(lock_);
    bitstream_converter_enabled_ = true;
  }
  current_stream()->EnableBitstreamConverter();
}

void AdaptiveDemuxerStream::OnAdaptiveDemuxerSeek(base::TimeDelta seek_time) {
  base::AutoLock auto_lock(lock_);

  last_buffer_timestamp_ = seek_time;

  // TODO(acolwell): Figure out what to do if this happens during a stream
  // switch.
}

// This method initiates a stream switch. The diagram below shows the steps
// involved.
//
//                           +-----------------------+
// ChangeCurrentStream() ->  | Store stream switch   |
//                           | index, seek_function, |
//                           |    and switch_cb.     |
//                           +-----------------------+
//                                   |
//                                   V
//                          +-------------------+ Yes
//                          | Are there pending | -----> (Wait for OnReadDone())
//                          | Read()s on the    |                |
//                          | current stream?   | <-----+        V
//                          +-------------------+       +--- OnReadDone()
//                                   | No
//                                   V
//                              StartSwitch()
//                                   |
//                                   V
//                        +------------------------+ No
//                        | Have buffer timestamp? |-----+
//                        +------------------------+     |
//                                   | Yes               |
//                                   V                   |
//                     +-----------------------------+   |
//                     | Seek stream to timestamp    |   |
//                     | using switch_seek_function_ |   |
//                     +-----------------------------+   |
//                                   |                   |
//                                   V                   |
//                              OnSwitchSeekDone() <-----+
//                                   |
//                                   V
//                         +------------------+ No
//                         | Seek successful? |----------+
//                         +------------------+          |
//                                   | Yes               |
//                                   V                   |
//                     +------------------------------+  |
//                     | Update current_stream_index_ |  |
//                     +------------------------------+  |
//                                   |                   |
//                                   V                   |
//                      +---------------------------+    |
//                      | Call Read() on new stream |<---+
//                      |    for deferred reads.    |
//                      +---------------------------+
//                                   |
//                                   V
//                            switch_cb(OK | ERROR)
//
// NOTE: Any AdaptiveDemuxerStream::Read() calls that occur during the stream
//       switch will be deferred until the switch has completed. The callbacks
//       will be queued on |read_cb_queue_|, but no Read() will be issued on the
//       current stream.
void AdaptiveDemuxerStream::ChangeCurrentStream(
    int index,
    const SeekFunction& seek_function,
    const PipelineStatusCB& switch_cb) {
  DCHECK_GE(index, 0);

  bool start_switch = false;

  {
    base::AutoLock auto_lock(lock_);

    DCHECK_LE(index, (int)streams_.size());
    DCHECK(streams_[index].get());
    DCHECK_NE(index, current_stream_index_);
    DCHECK(!IsSwitchPending_Locked());

    // TODO(acolwell): Still need to handle the case where the stream has ended.

    switch_cb_ = switch_cb;
    switch_index_ = index;
    switch_seek_function_ = seek_function;

    start_switch = CanStartSwitch_Locked();
  }

  if (start_switch)
    StartSwitch();
}

void AdaptiveDemuxerStream::OnReadDone(Buffer* buffer) {
  ReadCallback read_cb;
  bool start_switch = false;

  {
    base::AutoLock auto_lock(lock_);

    pending_reads_--;

    DCHECK_GE(pending_reads_, 0);
    DCHECK_GE(read_cb_queue_.size(), 0u);

    read_cb = read_cb_queue_.front();
    read_cb_queue_.pop_front();

    if (buffer && !buffer->IsEndOfStream())
      last_buffer_timestamp_ = buffer->GetTimestamp();

    start_switch = IsSwitchPending_Locked() && CanStartSwitch_Locked();
  }

  if (!read_cb.is_null())
    read_cb.Run(buffer);

  if (start_switch)
    StartSwitch();
}

bool AdaptiveDemuxerStream::IsSwitchPending_Locked() const {
  lock_.AssertAcquired();
  return !switch_cb_.is_null();
}

bool AdaptiveDemuxerStream::CanStartSwitch_Locked() const {
  lock_.AssertAcquired();

  if (pending_reads_ == 0) {
    DCHECK(IsSwitchPending_Locked());
    return true;
  }
  return false;
}

void AdaptiveDemuxerStream::StartSwitch() {
  SeekFunction seek_function;
  base::TimeDelta seek_point;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(IsSwitchPending_Locked());
    DCHECK_EQ(pending_reads_, 0);
    DCHECK_GE(switch_index_, 0);

    seek_point = last_buffer_timestamp_;
    seek_function = switch_seek_function_;

    // TODO(acolwell): add code to call switch_cb_ if we are at the end of the
    // stream now.
  }

  if (seek_point == kNoTimestamp) {
    // We haven't seen a buffer so there is no need to seek. Just move on to
    // the next stage in the switch process.
    OnSwitchSeekDone(kNoTimestamp, PIPELINE_OK);
    return;
  }

  DCHECK(!seek_function.is_null());
  seek_function.Run(seek_point,
                    base::Bind(&AdaptiveDemuxerStream::OnSwitchSeekDone, this));
}

void AdaptiveDemuxerStream::OnSwitchSeekDone(base::TimeDelta seek_time,
                                             PipelineStatus status) {
  DemuxerStream* stream = NULL;
  PipelineStatusCB switch_cb;
  int reads_to_request = 0;
  bool needs_bitstream_converter_enabled = false;

  {
    base::AutoLock auto_lock(lock_);

    if (status == PIPELINE_OK) {
      DCHECK(streams_[switch_index_]);

      current_stream_index_ = switch_index_;
      needs_bitstream_converter_enabled = bitstream_converter_enabled_;
    }

    // Clear stream switch state.
    std::swap(switch_cb, switch_cb_);
    switch_index_ = -1;
    switch_seek_function_.Reset();

    // Get the number of outstanding Read()s on this object.
    reads_to_request = read_cb_queue_.size();

    DCHECK_EQ(pending_reads_, 0);
    pending_reads_ = reads_to_request;

    stream = streams_[current_stream_index_];
  }

  if (needs_bitstream_converter_enabled)
    EnableBitstreamConverter();

  DCHECK(stream);
  DCHECK(!switch_cb.is_null());

  // Make the Read() calls that were deferred during the stream switch.
  for (;reads_to_request > 0; --reads_to_request)
    stream->Read(base::Bind(&AdaptiveDemuxerStream::OnReadDone, this));

  // Signal that the stream switch has completed.
  switch_cb.Run(status);
}

//
// AdaptiveDemuxer
//

AdaptiveDemuxer::AdaptiveDemuxer(DemuxerVector const& demuxers,
                                 int initial_audio_demuxer_index,
                                 int initial_video_demuxer_index)
    : demuxers_(demuxers),
      current_audio_demuxer_index_(initial_audio_demuxer_index),
      current_video_demuxer_index_(initial_video_demuxer_index),
      playback_rate_(0),
      switch_pending_(false),
      start_time_(kNoTimestamp) {
  DCHECK(!demuxers_.empty());
  DCHECK_GE(current_audio_demuxer_index_, -1);
  DCHECK_GE(current_video_demuxer_index_, -1);
  DCHECK_LT(current_audio_demuxer_index_, static_cast<int>(demuxers_.size()));
  DCHECK_LT(current_video_demuxer_index_, static_cast<int>(demuxers_.size()));
  DCHECK(current_audio_demuxer_index_ != -1 ||
         current_video_demuxer_index_ != -1);
  AdaptiveDemuxerStream::StreamVector audio_streams, video_streams;
  StreamSwitchManager::StreamIdVector video_ids;
  for (size_t i = 0; i < demuxers_.size(); ++i) {
    DemuxerStream* video = demuxers_[i]->GetStream(DemuxerStream::VIDEO);
    audio_streams.push_back(demuxers_[i]->GetStream(DemuxerStream::AUDIO));
    video_streams.push_back(video);
    if (video)
      video_ids.push_back(i);
    if (start_time_ == kNoTimestamp ||
        demuxers_[i]->GetStartTime() < start_time_) {
      start_time_ = demuxers_[i]->GetStartTime();
    }
  }
  DCHECK(start_time_ != kNoTimestamp);
  // TODO(fgalligan): Add a check to make sure |demuxers_| start time are
  // within an acceptable range, once the range is defined.
  if (current_audio_demuxer_index_ >= 0) {
    audio_stream_ = new AdaptiveDemuxerStream(
        audio_streams, current_audio_demuxer_index_);
  }
  if (current_video_demuxer_index_ >= 0) {
    video_stream_ = new AdaptiveDemuxerStream(
        video_streams, current_video_demuxer_index_);
  }

  stream_switch_manager_ = new StreamSwitchManager(this, video_ids);
  // TODO(fischman): any streams in the underlying demuxers that aren't being
  // consumed currently need to be sent to /dev/null or else FFmpegDemuxer will
  // hold data for them in memory, waiting for them to get drained by a
  // non-existent decoder.
}

AdaptiveDemuxer::~AdaptiveDemuxer() {}

// Switches the current video stream. The diagram below describes the switch
// process.
//                          +-------------------------------+
// ChangeVideoStream() ---> |          video_index          |
//                          |              ==               | Yes.
//                          | current_video_demuxer_index_? |--> done_cb(OK)
//                          +-------------------------------+
//                                          | No.
//                                          V
//                         Call video_stream_->ChangeCurrentStream()
//                                          |
//                                          V
//                          (Wait for ChangeVideoStreamDone())
//                                          |
//                                          V
//                                ChangeVideoStreamDone()
//                                          |
//                                          V
//                             +------------------------+ No
//                             | Was switch successful? |---> done_cb(ERROR)
//                             +------------------------+
//                                          |
//                                          V
//                            Update current_video_demuxer_index_
//                            & SetPlaybackRate() on new stream.
//                                          |
//                                          V
//                                      done_cb(OK).
//
void AdaptiveDemuxer::ChangeVideoStream(int video_index,
                                        const PipelineStatusCB& done_cb) {
  // TODO(fischman): this is currently broken because when a new Demuxer is to
  // be used we need to set_host(host()) it, and we need to set_host(NULL) the
  // current Demuxer if it's no longer being used.
  bool switching_to_current_stream = false;
  AdaptiveDemuxerStream::SeekFunction seek_function;

  {
    base::AutoLock auto_lock(lock_);

    DCHECK(video_stream_);
    DCHECK(!switch_pending_);
    if (current_video_demuxer_index_ == video_index) {
      switching_to_current_stream = true;
    } else {
      switch_pending_ = true;
      seek_function = base::Bind(&AdaptiveDemuxer::StartStreamSwitchSeek,
                                 this,
                                 DemuxerStream::VIDEO,
                                 video_index);
    }
  }

  if (switching_to_current_stream) {
    done_cb.Run(PIPELINE_OK);
    return;
  }

  video_stream_->ChangeCurrentStream(
      video_index, seek_function,
      base::Bind(&AdaptiveDemuxer::ChangeVideoStreamDone, this, video_index,
                 done_cb));
}

int AdaptiveDemuxer::GetCurrentVideoId() const {
  base::AutoLock auto_lock(lock_);
  return current_video_demuxer_index_;
}

void AdaptiveDemuxer::ChangeVideoStreamDone(int new_stream_index,
                                            const PipelineStatusCB& done_cb,
                                            PipelineStatus status) {
  {
    base::AutoLock auto_lock(lock_);

    switch_pending_ = false;

    if (status == PIPELINE_OK) {
      demuxers_[current_video_demuxer_index_]->SetPlaybackRate(0);
      current_video_demuxer_index_ = new_stream_index;
      demuxers_[current_video_demuxer_index_]->SetPlaybackRate(playback_rate_);
    }
  }

  done_cb.Run(status);
}

Demuxer* AdaptiveDemuxer::current_demuxer(DemuxerStream::Type type) {
  base::AutoLock auto_lock(lock_);
  switch (type) {
    case DemuxerStream::AUDIO:
      return (current_audio_demuxer_index_ < 0) ? NULL :
          demuxers_[current_audio_demuxer_index_];
    case DemuxerStream::VIDEO:
      return (current_video_demuxer_index_ < 0) ? NULL :
          demuxers_[current_video_demuxer_index_];
    default:
      LOG(DFATAL) << "Unexpected type: " << type;
      return NULL;
  }
}

// Helper class that wraps a FilterCallback and expects to get called a set
// number of times, after which the wrapped callback is fired (and deleted).
//
// TODO(acolwell): Remove this class once Stop() is converted to base::Closure.
class CountingCallback {
 public:
  CountingCallback(int count, FilterCallback* orig_cb)
      : remaining_count_(count), orig_cb_(orig_cb) {
    DCHECK_GT(remaining_count_, 0);
    DCHECK(orig_cb);
  }

  FilterCallback* GetACallback() {
    return NewCallback(this, &CountingCallback::OnChildCallbackDone);
  }

 private:
  void OnChildCallbackDone() {
    bool fire_orig_cb = false;
    {
      base::AutoLock auto_lock(lock_);
      if (--remaining_count_ == 0)
        fire_orig_cb = true;
    }
    if (fire_orig_cb) {
      orig_cb_->Run();
      delete this;
    }
  }

  base::Lock lock_;
  int remaining_count_;
  scoped_ptr<FilterCallback> orig_cb_;
};

// Helper class that wraps PipelineStatusCB and expects to get called a set
// number of times, after which the wrapped callback is fired. If an error
// is reported in any of the callbacks, only the first error code is passed
// to the wrapped callback.
class CountingStatusCB : public base::RefCountedThreadSafe<CountingStatusCB> {
 public:
  CountingStatusCB(int count, const PipelineStatusCB& orig_cb)
      : remaining_count_(count), orig_cb_(orig_cb),
        overall_status_(PIPELINE_OK) {
    DCHECK_GT(remaining_count_, 0);
    DCHECK(!orig_cb.is_null());
  }

  PipelineStatusCB GetACallback() {
    return base::Bind(&CountingStatusCB::OnChildCallbackDone, this);
  }

 private:
  void OnChildCallbackDone(PipelineStatus status) {
    bool fire_orig_cb = false;
    PipelineStatus overall_status = PIPELINE_OK;

    {
      base::AutoLock auto_lock(lock_);

      if (overall_status_ == PIPELINE_OK && status != PIPELINE_OK)
        overall_status_ = status;

      if (--remaining_count_ == 0) {
        fire_orig_cb = true;
        overall_status = overall_status_;
      }
    }

    if (fire_orig_cb)
      orig_cb_.Run(overall_status);
  }

  base::Lock lock_;
  int remaining_count_;
  PipelineStatusCB orig_cb_;
  PipelineStatus overall_status_;

  DISALLOW_COPY_AND_ASSIGN(CountingStatusCB);
};

void AdaptiveDemuxer::Stop(FilterCallback* callback) {
  stream_switch_manager_->Stop();

  // Stop() must be called on all of the demuxers even though only one demuxer
  // is  actively delivering audio and another one is delivering video. This
  // just satisfies the contract that all demuxers must have Stop() called on
  // them before they are destroyed.
  //
  // TODO(acolwell): Remove CountingCallback once Stop() is converted to
  // base::Closure.
  CountingCallback* wrapper = new CountingCallback(demuxers_.size(), callback);
  for (size_t i = 0; i < demuxers_.size(); ++i)
    demuxers_[i]->Stop(wrapper->GetACallback());
}

void AdaptiveDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  if (audio_stream_)
    audio_stream_->OnAdaptiveDemuxerSeek(time);

  if (video_stream_)
    video_stream_->OnAdaptiveDemuxerSeek(time);

  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  int count = (audio ? 1 : 0) + (video && audio != video ? 1 : 0);
  CountingStatusCB* wrapper = new CountingStatusCB(count, cb);
  if (audio)
    audio->Seek(time, wrapper->GetACallback());
  if (video && audio != video)
    video->Seek(time, wrapper->GetACallback());
}

void AdaptiveDemuxer::OnAudioRendererDisabled() {
  for (size_t i = 0; i < demuxers_.size(); ++i)
    demuxers_[i]->OnAudioRendererDisabled();
}

void AdaptiveDemuxer::set_host(FilterHost* filter_host) {
  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  if (audio) audio->set_host(filter_host);
  if (video && audio != video) video->set_host(filter_host);
}

void AdaptiveDemuxer::SetPlaybackRate(float playback_rate) {
  {
    base::AutoLock auto_lock(lock_);
    if (playback_rate_ == 0 && playback_rate > 0) {
      stream_switch_manager_->Play();
    } else if (playback_rate_ > 0 && playback_rate == 0) {
      stream_switch_manager_->Pause();
    }

    playback_rate_ = playback_rate;
  }

  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  if (audio) audio->SetPlaybackRate(playback_rate);
  if (video && audio != video) video->SetPlaybackRate(playback_rate);
}

void AdaptiveDemuxer::SetPreload(Preload preload) {
  for (size_t i = 0; i < demuxers_.size(); ++i)
    demuxers_[i]->SetPreload(preload);
}

scoped_refptr<DemuxerStream> AdaptiveDemuxer::GetStream(
    DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return audio_stream_;
    case DemuxerStream::VIDEO:
      return video_stream_;
    default:
      LOG(DFATAL) << "Unexpected type " << type;
      return NULL;
  }
}

base::TimeDelta AdaptiveDemuxer::GetStartTime() const {
  return start_time_;
}

void AdaptiveDemuxer::StartStreamSwitchSeek(
    DemuxerStream::Type type,
    int stream_index,
    base::TimeDelta seek_time,
    const AdaptiveDemuxerStream::SeekFunctionCB& seek_cb) {
  DCHECK_GE(stream_index, 0);
  DCHECK(!seek_cb.is_null());

  Demuxer* demuxer = NULL;
  base::TimeDelta seek_point;
  PipelineStatusCB filter_cb;

  {
    base::AutoLock auto_lock(lock_);

    demuxer = demuxers_[stream_index];

    if (GetSeekTimeAfter(demuxer->GetStream(type)->GetAVStream(), seek_time,
                         &seek_point)) {
      // We found a seek point.
      filter_cb = base::Bind(&AdaptiveDemuxer::OnStreamSeekDone,
                             seek_cb, seek_point);
    } else {
      // We didn't find a seek point. Assume we don't have index data for it
      // yet. Seek to the specified time to force index data to be loaded.
      seek_point = seek_time;
      filter_cb = base::Bind(&AdaptiveDemuxer::OnIndexSeekDone, this,
                             type, stream_index, seek_time, seek_cb);
    }
  }

  DCHECK(demuxer);
  demuxer->Seek(seek_point, filter_cb);
}

void AdaptiveDemuxer::OnIndexSeekDone(
    DemuxerStream::Type type,
    int stream_index,
    base::TimeDelta seek_time,
    const AdaptiveDemuxerStream::SeekFunctionCB& seek_cb,
    PipelineStatus status) {
  base::TimeDelta seek_point;
  PipelineStatusCB filter_cb;

  Demuxer* demuxer = NULL;

  if (status != PIPELINE_OK) {
    seek_cb.Run(base::TimeDelta(), status);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);

    demuxer = demuxers_[stream_index];
    DCHECK(demuxer);

    // Look for a seek point now that we have index data.
    if (GetSeekTimeAfter(demuxer->GetStream(type)->GetAVStream(), seek_time,
                         &seek_point)) {
      filter_cb = base::Bind(&AdaptiveDemuxer::OnStreamSeekDone,
                             seek_cb, seek_point);
    } else {
      // Failed again to find a seek point. Clear the demuxer so that
      // a seek error will be reported.
      demuxer = NULL;
    }
  }

  if (!demuxer) {
    seek_cb.Run(base::TimeDelta(), PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  demuxer->Seek(seek_point, filter_cb);
}

// static
void AdaptiveDemuxer::OnStreamSeekDone(
    const AdaptiveDemuxerStream::SeekFunctionCB& seek_cb,
    base::TimeDelta seek_point,
    PipelineStatus status) {
  seek_cb.Run(seek_point, status);
}

//
// AdaptiveDemuxerFactory
//

AdaptiveDemuxerFactory::AdaptiveDemuxerFactory(
    DemuxerFactory* delegate_factory)
    : delegate_factory_(delegate_factory) {
  DCHECK(delegate_factory);
}

AdaptiveDemuxerFactory::~AdaptiveDemuxerFactory() {}

DemuxerFactory* AdaptiveDemuxerFactory::Clone() const {
  return new AdaptiveDemuxerFactory(delegate_factory_->Clone());
}

// See AdaptiveDemuxerFactory's class-level comment for |url|'s format.
bool ParseAdaptiveUrl(
    const std::string& url, int* audio_index, int* video_index,
    std::vector<std::string>* urls) {
  urls->clear();

  if (url.empty())
    return false;
  if (!StartsWithASCII(url, "x-adaptive:", false)) {
    return ParseAdaptiveUrl(
        "x-adaptive:0:0:" + url, audio_index, video_index, urls);
  }

  std::vector<std::string> parts;
  base::SplitStringDontTrim(url, ':', &parts);
  if (parts.size() < 4 ||
      parts[0] != "x-adaptive" ||
      !base::StringToInt(parts[1], audio_index) ||
      !base::StringToInt(parts[2], video_index) ||
      *audio_index < -1 || *video_index < -1) {
    return false;
  }

  std::string::size_type first_url_pos =
      parts[0].size() + 1 + parts[1].size() + 1 + parts[2].size() + 1;
  std::string urls_str = url.substr(first_url_pos);
  if (urls_str.empty())
    return false;
  base::SplitStringDontTrim(urls_str, '^', urls);
  if (urls->empty() ||
      *audio_index >= static_cast<int>(urls->size()) ||
      *video_index >= static_cast<int>(urls->size())) {
    return false;
  }

  return true;
}

// Wrapper for a BuildCallback which accumulates the Demuxer's returned by a
// number of DemuxerFactory::Build() calls and eventually constructs an
// AdaptiveDemuxer and returns it to the |orig_cb| (or errors out if any
// individual Demuxer fails construction).
class DemuxerAccumulator {
 public:
  // Takes ownership of |orig_cb|.
  DemuxerAccumulator(int audio_index, int video_index,
                     int count, DemuxerFactory::BuildCallback* orig_cb)
      : audio_index_(audio_index), video_index_(video_index),
        remaining_count_(count), orig_cb_(orig_cb),
        demuxers_(count, static_cast<Demuxer*>(NULL)),
        statuses_(count, PIPELINE_OK) {
    DCHECK_GT(remaining_count_, 0);
    DCHECK(orig_cb_.get());
  }

  DemuxerFactory::BuildCallback* GetNthCallback(int n) {
    return new IndividualCallback(this, n);
  }

 private:
  // Wrapper for a BuildCallback that can carry one extra piece of data: the
  // index of this callback in the original list of outstanding requests.
  struct IndividualCallback : public DemuxerFactory::BuildCallback {
    IndividualCallback(DemuxerAccumulator* accumulator, int index)
        : accumulator_(accumulator), index_(index) {}

    virtual void RunWithParams(const Tuple2<PipelineStatus, Demuxer*>& params) {
      accumulator_->Run(index_, params.a, params.b);
    }
    DemuxerAccumulator* accumulator_;
    int index_;
  };

  // When an individual callback is fired, it calls this method.
  void Run(int index, PipelineStatus status, Demuxer* demuxer) {
    bool fire_orig_cb = false;
    {
      base::AutoLock auto_lock(lock_);
      DCHECK(!demuxers_[index]);
      demuxers_[index] = demuxer;
      statuses_[index] = status;
      if (--remaining_count_ == 0)
        fire_orig_cb = true;
    }
    if (fire_orig_cb)
      DoneAccumulating();
  }

  // Called when all of the demuxers have been built.
  void DoneAccumulating() {
    PipelineStatus overall_status = GetOverallStatus();
    if (overall_status == PIPELINE_OK) {
      CallOriginalCallback(PIPELINE_OK,
                           new AdaptiveDemuxer(demuxers_, audio_index_,
                                               video_index_));
      return;
    }

    // An error occurred. Call Stop() on all of the demuxers that were
    // successfully built.
    AdaptiveDemuxer::DemuxerVector demuxers_to_stop;

    for (size_t i = 0; i < demuxers_.size(); ++i) {
      if (demuxers_[i].get())
        demuxers_to_stop.push_back(demuxers_[i]);
    }

    if (demuxers_to_stop.empty()) {
      CallOriginalCallback(overall_status, NULL);
      return;
    }

    CountingCallback* wrapper = new CountingCallback(
        demuxers_to_stop.size(),
        NewCallback(this, &DemuxerAccumulator::StopOnErrorDone));
    for (size_t i = 0; i < demuxers_to_stop.size(); ++i)
      demuxers_to_stop[i]->Stop(wrapper->GetACallback());
  }

  // Called after Stop() has been called on all of the demuxers that were
  // successfully built.
  void StopOnErrorDone() {
    CallOriginalCallback(GetOverallStatus(), NULL);
  }

  void CallOriginalCallback(PipelineStatus status, Demuxer* demuxer) {
    orig_cb_->Run(status, demuxer);

    delete this;
  }

  // Gets the overall status of the demuxer factory builds. If all build
  // operations are successful then this will return PIPELINE_OK. If there
  // are any errors then this function will return the first error that
  // it encounters. Multiple builds may have failed, but we only care about
  // the first error since only one error can be reported to the higher level
  // code.
  PipelineStatus GetOverallStatus() const {
    for (size_t i = 0; i < statuses_.size(); ++i) {
      if (statuses_[i] != PIPELINE_OK)
        return statuses_[i];
    }

    return PIPELINE_OK;
  }

  // Self-delete in CallOriginalCallback() only.
  ~DemuxerAccumulator() {}

  int audio_index_;
  int video_index_;
  int remaining_count_;
  scoped_ptr<DemuxerFactory::BuildCallback> orig_cb_;
  base::Lock lock_;  // Guards vectors of results below.
  AdaptiveDemuxer::DemuxerVector demuxers_;
  std::vector<PipelineStatus> statuses_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DemuxerAccumulator);
};

void AdaptiveDemuxerFactory::Build(const std::string& url, BuildCallback* cb) {
  std::vector<std::string> urls;
  int audio_index, video_index;
  if (!ParseAdaptiveUrl(url, &audio_index, &video_index, &urls)) {
    cb->Run(Tuple2<PipelineStatus, Demuxer*>(
        DEMUXER_ERROR_COULD_NOT_OPEN, NULL));
    delete cb;
    return;
  }
  DemuxerAccumulator* accumulator = new DemuxerAccumulator(
      audio_index, video_index, urls.size(), cb);
  for (size_t i = 0; i < urls.size(); ++i)
    delegate_factory_->Build(urls[i], accumulator->GetNthCallback(i));
}

}  // namespace media
