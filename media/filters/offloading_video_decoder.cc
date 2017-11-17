// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/offloading_video_decoder.h"

#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"

namespace media {

static void ReleaseDecoder(std::unique_ptr<VideoDecoder> decoder) {}

OffloadingVideoDecoder::OffloadingVideoDecoder(
    int min_offloading_width,
    std::vector<VideoCodec> supported_codecs,
    std::unique_ptr<OffloadableVideoDecoder> decoder)
    : min_offloading_width_(min_offloading_width),
      supported_codecs_(std::move(supported_codecs)),
      decoder_(std::move(decoder)),
      weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

OffloadingVideoDecoder::~OffloadingVideoDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (offload_task_runner_) {
    // Can't use DeleteSoon() here since VideoDecoder has a custom deleter.
    offload_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReleaseDecoder, base::Passed(&decoder_)));
  }
}

std::string OffloadingVideoDecoder::GetDisplayName() const {
  // This call is expected to be static and safe to call from any thread.
  return decoder_->GetDisplayName();
}

void OffloadingVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        const InitCB& init_cb,
                                        const OutputCB& output_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(config.IsValidConfig());

  const bool disable_offloading =
      config.is_encrypted() ||
      config.coded_size().width() < min_offloading_width_ ||
      std::find(supported_codecs_.begin(), supported_codecs_.end(),
                config.codec()) == supported_codecs_.end();

  if (initialized_) {
    initialized_ = false;

    // We're transitioning from offloading to no offloading, so detach from the
    // offloading thread so we can run on the media thread.
    if (disable_offloading && offload_task_runner_) {
      offload_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&OffloadableVideoDecoder::Detach,
                         base::Unretained(decoder_.get())),
          // We must trampoline back trough OffloadingVideoDecoder because it's
          // possible for this class to be destroyed during Initialize().
          base::BindOnce(&OffloadingVideoDecoder::Initialize,
                         weak_factory_.GetWeakPtr(), config, low_delay,
                         cdm_context, init_cb, output_cb));
      return;
    }

    // We're transitioning from no offloading to offloading, so detach from the
    // media thread so we can run on the offloading thread.
    if (!disable_offloading && !offload_task_runner_)
      decoder_->Detach();
  }

  DCHECK(!initialized_);
  initialized_ = true;

  // Offloaded decoders expect asynchronous execution of callbacks; even if we
  // aren't currently using the offload thread.
  InitCB bound_init_cb = BindToCurrentLoop(init_cb);
  OutputCB bound_output_cb = BindToCurrentLoop(output_cb);

  // If we're not offloading just pass through to the wrapped decoder.
  if (disable_offloading) {
    offload_task_runner_ = nullptr;
    decoder_->Initialize(config, low_delay, cdm_context, bound_init_cb,
                         bound_output_cb);
    return;
  }

  if (!offload_task_runner_) {
    offload_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::TaskPriority::USER_BLOCKING});
  }

  offload_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OffloadableVideoDecoder::Initialize,
                     base::Unretained(decoder_.get()), config, low_delay,
                     cdm_context, bound_init_cb, bound_output_cb));
}

void OffloadingVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(!decode_cb.is_null());

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);
  if (!offload_task_runner_) {
    decoder_->Decode(buffer, bound_decode_cb);
    return;
  }

  offload_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OffloadableVideoDecoder::Decode,
                                base::Unretained(decoder_.get()), buffer,
                                bound_decode_cb));
}

void OffloadingVideoDecoder::Reset(const base::Closure& reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::Closure bound_reset_cb = BindToCurrentLoop(reset_cb);
  if (!offload_task_runner_) {
    decoder_->Reset(bound_reset_cb);
    return;
  }

  offload_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OffloadableVideoDecoder::Reset,
                     base::Unretained(decoder_.get()), bound_reset_cb));
}

}  // namespace media
