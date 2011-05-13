// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "media/filters/adaptive_demuxer.h"

namespace media {

//
// AdaptiveDemuxerStream
//
AdaptiveDemuxerStream::AdaptiveDemuxerStream(
    StreamVector const& streams, int initial_stream)
    : streams_(streams), current_stream_index_(initial_stream),
      bitstream_converter_enabled_(false) {
  DCheckSanity();
}

void AdaptiveDemuxerStream::DCheckSanity() {
  // We only carry out sanity checks in debug mode.
  if (!logging::DEBUG_MODE)
    return;
  DCHECK(streams_[current_stream_index_].get());

  bool non_null_stream_seen = false;
  Type type = DemuxerStream::UNKNOWN;
  const MediaFormat* media_format = NULL;
  for (size_t i = 0; i < streams_.size(); ++i) {
    if (!streams_[i])
      continue;

    if (!non_null_stream_seen) {
      non_null_stream_seen = true;
      type = streams_[i]->type();
      media_format = &streams_[i]->media_format();
    } else {
      DCHECK_EQ(streams_[i]->type(), type);
      DCHECK(streams_[i]->media_format() == *media_format);
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
  current_stream()->Read(read_callback);
}

AVStream* AdaptiveDemuxerStream::GetAVStream() {
  return current_stream()->GetAVStream();
}

DemuxerStream::Type AdaptiveDemuxerStream::type() {
  return current_stream()->type();
}

const MediaFormat& AdaptiveDemuxerStream::media_format() {
  return current_stream()->media_format();
}

void AdaptiveDemuxerStream::EnableBitstreamConverter() {
  {
    base::AutoLock auto_lock(lock_);
    bitstream_converter_enabled_ = true;
  }
  current_stream()->EnableBitstreamConverter();
}

void AdaptiveDemuxerStream::ChangeCurrentStream(int index) {
  bool needs_bitstream_converter_enabled;
  {
    base::AutoLock auto_lock(lock_);
    current_stream_index_ = index;
    DCHECK(streams_[current_stream_index_]);
    needs_bitstream_converter_enabled = bitstream_converter_enabled_;
  }
  if (needs_bitstream_converter_enabled)
    EnableBitstreamConverter();
}

//
// AdaptiveDemuxer
//

AdaptiveDemuxer::AdaptiveDemuxer(DemuxerVector const& demuxers,
                                 int initial_audio_demuxer_index,
                                 int initial_video_demuxer_index)
    : demuxers_(demuxers),
      current_audio_demuxer_index_(initial_audio_demuxer_index),
      current_video_demuxer_index_(initial_video_demuxer_index) {
  DCHECK(!demuxers_.empty());
  DCHECK_GE(current_audio_demuxer_index_, -1);
  DCHECK_GE(current_video_demuxer_index_, -1);
  DCHECK_LT(current_audio_demuxer_index_, static_cast<int>(demuxers_.size()));
  DCHECK_LT(current_video_demuxer_index_, static_cast<int>(demuxers_.size()));
  DCHECK(current_audio_demuxer_index_ != -1 ||
         current_video_demuxer_index_ != -1);
  AdaptiveDemuxerStream::StreamVector audio_streams, video_streams;
  for (size_t i = 0; i < demuxers_.size(); ++i) {
    audio_streams.push_back(demuxers_[i]->GetStream(DemuxerStream::AUDIO));
    video_streams.push_back(demuxers_[i]->GetStream(DemuxerStream::VIDEO));
  }
  if (current_audio_demuxer_index_ >= 0) {
    audio_stream_ = new AdaptiveDemuxerStream(
        audio_streams, current_audio_demuxer_index_);
  }
  if (current_video_demuxer_index_ >= 0) {
    video_stream_ = new AdaptiveDemuxerStream(
        video_streams, current_video_demuxer_index_);
  }

  // TODO(fischman): any streams in the underlying demuxers that aren't being
  // consumed currently need to be sent to /dev/null or else FFmpegDemuxer will
  // hold data for them in memory, waiting for them to get drained by a
  // non-existent decoder.
}

AdaptiveDemuxer::~AdaptiveDemuxer() {}

void AdaptiveDemuxer::ChangeCurrentDemuxer(int audio_index, int video_index) {
  // TODO(fischman): this is currently broken because when a new Demuxer is to
  // be used we need to set_host(host()) it, and we need to set_host(NULL) the
  // current Demuxer if it's no longer being used.
  base::AutoLock auto_lock(lock_);
  current_audio_demuxer_index_ = audio_index;
  current_video_demuxer_index_ = video_index;
  if (audio_stream_)
    audio_stream_->ChangeCurrentStream(audio_index);
  if (video_stream_)
    video_stream_->ChangeCurrentStream(video_index);
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
// TODO: Remove this class once Stop() is converted to FilterStatusCB.
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

// Helper class that wraps FilterStatusCB and expects to get called a set
// number of times, after which the wrapped callback is fired. If an error
// is reported in any of the callbacks, only the first error code is passed
// to the wrapped callback.
class CountingStatusCB : public base::RefCountedThreadSafe<CountingStatusCB> {
 public:
  CountingStatusCB(int count, const FilterStatusCB& orig_cb)
      : remaining_count_(count), orig_cb_(orig_cb),
        overall_status_(PIPELINE_OK) {
    DCHECK_GT(remaining_count_, 0);
    DCHECK(!orig_cb.is_null());
  }

  FilterStatusCB GetACallback() {
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
  FilterStatusCB orig_cb_;
  PipelineStatus overall_status_;

  DISALLOW_COPY_AND_ASSIGN(CountingStatusCB);
};

void AdaptiveDemuxer::Stop(FilterCallback* callback) {
  // Stop() must be called on all of the demuxers even though only one demuxer
  // is  actively delivering audio and another one is delivering video. This
  // just satisfies the contract that all demuxers must have Stop() called on
  // them before they are destroyed.
  //
  // TODO: Remove CountingCallback once Stop() is converted to FilterStatusCB.
  CountingCallback* wrapper = new CountingCallback(demuxers_.size(), callback);
  for (size_t i = 0; i < demuxers_.size(); ++i)
    demuxers_[i]->Stop(wrapper->GetACallback());
}

void AdaptiveDemuxer::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
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
