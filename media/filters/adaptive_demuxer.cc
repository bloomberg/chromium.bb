// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

void AdaptiveDemuxerStream::Read(Callback1<Buffer*>::Type* read_callback) {
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
  // TODO(fischman): remember to Stop active demuxers that are being abandoned.
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

void AdaptiveDemuxer::Stop(FilterCallback* callback) {
  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  int count = (audio ? 1 : 0) + (video && audio != video ? 1 : 0);
  CountingCallback* wrapper = new CountingCallback(count, callback);
  if (audio)
    audio->Stop(wrapper->GetACallback());
  if (video && audio != video)
    video->Stop(wrapper->GetACallback());
}

void AdaptiveDemuxer::Seek(base::TimeDelta time, FilterCallback* callback) {
  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  int count = (audio ? 1 : 0) + (video && audio != video ? 1 : 0);
  CountingCallback* wrapper = new CountingCallback(count, callback);
  if (audio)
    audio->Seek(time, wrapper->GetACallback());
  if (video && audio != video)
    video->Seek(time, wrapper->GetACallback());
}

void AdaptiveDemuxer::OnAudioRendererDisabled() {
  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  if (audio) audio->OnAudioRendererDisabled();
  if (video && audio != video) video->OnAudioRendererDisabled();
  // TODO(fischman): propagate to other demuxers if/when they're selected.
}

void AdaptiveDemuxer::set_host(FilterHost* filter_host) {
  Demuxer* audio = current_demuxer(DemuxerStream::AUDIO);
  Demuxer* video = current_demuxer(DemuxerStream::VIDEO);
  if (audio) audio->set_host(filter_host);
  if (video && audio != video) video->set_host(filter_host);
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

  void DoneAccumulating() {
    PipelineStatus overall_status = PIPELINE_OK;
    for (size_t i = 0; i < statuses_.size(); ++i) {
      if (statuses_[i] != PIPELINE_OK) {
        overall_status = statuses_[i];
        break;
      }
    }
    if (overall_status == PIPELINE_OK) {
      orig_cb_->Run(
          PIPELINE_OK,
          new AdaptiveDemuxer(demuxers_, audio_index_, video_index_));
    } else {
      orig_cb_->Run(overall_status, static_cast<Demuxer*>(NULL));
    }

    delete this;
  }

  // Self-delete in DoneAccumulating() only.
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
