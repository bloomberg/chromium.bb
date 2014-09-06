// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/filters/demuxer_stream_adapter.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/media_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"

namespace chromecast {
namespace media {

namespace {

class DummyMediaTaskRunner : public MediaTaskRunner {
 public:
  DummyMediaTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // MediaTaskRunner implementation.
  virtual bool PostMediaTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta timestamp) OVERRIDE;

 private:
  virtual ~DummyMediaTaskRunner();

  scoped_refptr<base::SingleThreadTaskRunner> const task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DummyMediaTaskRunner);
};

DummyMediaTaskRunner::DummyMediaTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
  : task_runner_(task_runner) {
}

DummyMediaTaskRunner::~DummyMediaTaskRunner() {
}

bool DummyMediaTaskRunner::PostMediaTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta timestamp) {
  return task_runner_->PostTask(from_here, task);
}

}  // namespace

DemuxerStreamAdapter::DemuxerStreamAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<BalancedMediaTaskRunnerFactory>&
    media_task_runner_factory,
    ::media::DemuxerStream* demuxer_stream)
    : task_runner_(task_runner),
      media_task_runner_factory_(media_task_runner_factory),
      media_task_runner_(new DummyMediaTaskRunner(task_runner)),
      demuxer_stream_(demuxer_stream),
      is_pending_read_(false),
      weak_factory_(new base::WeakPtrFactory<DemuxerStreamAdapter>(this)) {
  ResetMediaTaskRunner();
  weak_this_ = weak_factory_->GetWeakPtr();
  thread_checker_.DetachFromThread();
}

DemuxerStreamAdapter::~DemuxerStreamAdapter() {
  // Needed since we use weak pointers:
  // weak pointers must be invalidated on the same thread.
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DemuxerStreamAdapter::Read(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Support only one read at a time.
  DCHECK(!is_pending_read_);

  is_pending_read_ = true;
  bool may_run_in_future = media_task_runner_->PostMediaTask(
      FROM_HERE,
      base::Bind(&DemuxerStreamAdapter::RequestBuffer, weak_this_, read_cb),
      max_pts_);
  DCHECK(may_run_in_future);
}

void DemuxerStreamAdapter::Flush(const base::Closure& flush_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CMALOG(kLogControl) << __FUNCTION__;

  // Invalidate all the weak pointers so that we don't receive anymore buffers
  // from |demuxer_stream_| that are associated with the current media
  // timeline.
  is_pending_read_ = false;
  weak_factory_->InvalidateWeakPtrs();
  weak_factory_.reset(new base::WeakPtrFactory<DemuxerStreamAdapter>(this));
  weak_this_ = weak_factory_->GetWeakPtr();

  // Reset the decoder configurations.
  audio_config_ = ::media::AudioDecoderConfig();
  video_config_ = ::media::VideoDecoderConfig();

  // Create a new media task runner for the upcoming media playback.
  ResetMediaTaskRunner();

  CMALOG(kLogControl) << "Flush done";
  flush_cb.Run();
}

void DemuxerStreamAdapter::ResetMediaTaskRunner() {
  DCHECK(thread_checker_.CalledOnValidThread());

  max_pts_ = ::media::kNoTimestamp();
  if (media_task_runner_factory_.get()) {
    media_task_runner_ =
        media_task_runner_factory_->CreateMediaTaskRunner(task_runner_);
  }
}

void DemuxerStreamAdapter::RequestBuffer(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  demuxer_stream_->Read(::media::BindToCurrentLoop(
      base::Bind(&DemuxerStreamAdapter::OnNewBuffer, weak_this_, read_cb)));
}

void DemuxerStreamAdapter::OnNewBuffer(
    const ReadCB& read_cb,
    ::media::DemuxerStream::Status status,
    const scoped_refptr< ::media::DecoderBuffer>& input) {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_pending_read_ = false;

  if (status == ::media::DemuxerStream::kAborted) {
    DCHECK(input.get() == NULL);
    return;
  }

  if (status == ::media::DemuxerStream::kConfigChanged) {
    DCHECK(input.get() == NULL);
    if (demuxer_stream_->type() == ::media::DemuxerStream::VIDEO)
      video_config_ = demuxer_stream_->video_decoder_config();
    if (demuxer_stream_->type() == ::media::DemuxerStream::AUDIO)
      audio_config_ = demuxer_stream_->audio_decoder_config();

    // Got a new config, but we still need to get a frame.
    Read(read_cb);
    return;
  }

  DCHECK_EQ(status, ::media::DemuxerStream::kOk);

  // Updates the timestamp used for task scheduling.
  if (!input->end_of_stream() &&
      input->timestamp() != ::media::kNoTimestamp() &&
      (max_pts_ == ::media::kNoTimestamp() || input->timestamp() > max_pts_)) {
    max_pts_ = input->timestamp();
  }

  // Provides the buffer as well as possibly valid audio and video configs.
  scoped_refptr<DecoderBufferBase> buffer(new DecoderBufferAdapter(input));
  read_cb.Run(buffer, audio_config_, video_config_);

  // Back to the default audio/video config:
  // an invalid audio/video config means there is no config update.
  if (audio_config_.IsValidConfig())
    audio_config_ = ::media::AudioDecoderConfig();
  if (video_config_.IsValidConfig())
    video_config_ = ::media::VideoDecoderConfig();
}

}  // namespace media
}  // namespace chromecast
