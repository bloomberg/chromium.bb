// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/video_decoder_pipeline.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/linux/dmabuf_video_frame_pool.h"
#include "media/gpu/macros.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_slice_video_decoder.h"
#endif

namespace media {

// static
base::queue<VideoDecoderPipeline::CreateVDFunc>
VideoDecoderPipeline::GetCreateVDFunctions(
    VideoDecoderPipeline::CreateVDFunc cur_create_vd_func) {
  static constexpr VideoDecoderPipeline::CreateVDFunc kCreateVDFuncs[] = {
#if BUILDFLAG(USE_V4L2_CODEC)
    &V4L2SliceVideoDecoder::Create,
#endif  // BUILDFLAG(USE_V4L2_CODEC)

#if BUILDFLAG(USE_VAAPI)
    &VaapiVideoDecoder::Create,
#endif  // BUILDFLAG(USE_VAAPI)
  };

  base::queue<VideoDecoderPipeline::CreateVDFunc> ret;
  for (const auto& func : kCreateVDFuncs) {
    if (func != cur_create_vd_func)
      ret.push(func);
  }
  return ret;
}

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter) {
  if (!client_task_runner || !frame_pool || !frame_converter) {
    VLOGF(1) << "One of arguments is nullptr.";
    return nullptr;
  }

  if (GetCreateVDFunctions(nullptr).empty()) {
    VLOGF(1) << "No available function to create video decoder.";
    return nullptr;
  }

  return base::WrapUnique<VideoDecoder>(new VideoDecoderPipeline(
      std::move(client_task_runner), std::move(frame_pool),
      std::move(frame_converter)));
}

VideoDecoderPipeline::VideoDecoderPipeline(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter)
    : client_task_runner_(std::move(client_task_runner)),
      decoder_task_runner_(base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_VISIBLE},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      frame_pool_(std::move(frame_pool)),
      frame_converter_(std::move(frame_converter)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  DCHECK(frame_pool_);
  DCHECK(frame_converter_);
  DCHECK(client_task_runner_);
  DVLOGF(2);

  weak_this_ = weak_this_factory_.GetWeakPtr();
  frame_pool_->set_parent_task_runner(decoder_task_runner_);
  frame_converter_->Initialize(
      client_task_runner_,
      base::BindRepeating(&VideoDecoderPipeline::OnFrameConverted, weak_this_));
}

VideoDecoderPipeline::~VideoDecoderPipeline() {
  // We have to destroy |frame_pool_| on |decoder_task_runner_|, so the
  // destructor is also called on |decoder_task_runner_|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
}

void VideoDecoderPipeline::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(2);

  weak_this_factory_.InvalidateWeakPtrs();

  decoder_.reset();
  used_create_vd_func_ = nullptr;
  frame_converter_.reset();

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderPipeline::DestroyTask,
                                base::Unretained(this)));
}

void VideoDecoderPipeline::DestroyTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // |frame_pool_| should be destroyed on |decoder_task_runner_|, which is set
  // by frame_pool_->set_parent_task_runner().
  frame_pool_.reset();
  delete this;
}

std::string VideoDecoderPipeline::GetDisplayName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return "VideoDecoderPipeline";
}

bool VideoDecoderPipeline::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return true;
}

int VideoDecoderPipeline::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!decoder_)
    DVLOGF(1) << "Call before Initialize() success.";
  return decoder_ ? decoder_->GetMaxDecodeRequests() : 1;
}

bool VideoDecoderPipeline::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!decoder_)
    DVLOGF(1) << "Call before Initialize() success.";
  return decoder_ ? decoder_->NeedsBitstreamConversion() : false;
}

bool VideoDecoderPipeline::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!decoder_)
    DVLOGF(1) << "Call before Initialize() success.";
  return decoder_ ? decoder_->CanReadWithoutStalling() : false;
}

void VideoDecoderPipeline::Initialize(const VideoDecoderConfig& config,
                                      bool low_delay,
                                      CdmContext* cdm_context,
                                      InitCB init_cb,
                                      const OutputCB& output_cb,
                                      const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(!init_cb_);
  VLOGF(2) << "config: " << config.AsHumanReadableString();

  client_output_cb_ = std::move(output_cb);
  init_cb_ = std::move(init_cb);
  base::queue<VideoDecoderPipeline::CreateVDFunc> create_vd_funcs =
      GetCreateVDFunctions(used_create_vd_func_);

  if (!decoder_) {
    CreateAndInitializeVD(std::move(create_vd_funcs), config, low_delay,
                          cdm_context, waiting_cb);
  } else {
    decoder_->Initialize(
        config, low_delay, cdm_context,
        // If it fails to re-initialize current |decoder_|, it will create
        // another decoder instance by trying available VD creation functions
        // again. See |OnInitializeDone| for detail.
        base::BindOnce(&VideoDecoderPipeline::OnInitializeDone, weak_this_,
                       std::move(create_vd_funcs), config, low_delay,
                       cdm_context, waiting_cb),
        base::BindRepeating(&VideoDecoderPipeline::OnFrameDecodedThunk,
                            client_task_runner_, weak_this_),
        waiting_cb);
  }
}

void VideoDecoderPipeline::CreateAndInitializeVD(
    base::queue<VideoDecoderPipeline::CreateVDFunc> create_vd_funcs,
    VideoDecoderConfig config,
    bool low_delay,
    CdmContext* cdm_context,
    WaitingCB waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(init_cb_);
  DCHECK(!decoder_);
  DCHECK(!used_create_vd_func_);
  DVLOGF(3);

  if (create_vd_funcs.empty()) {
    DVLOGF(2) << "No available video decoder.";
    std::move(init_cb_).Run(false);
    return;
  }

  used_create_vd_func_ = create_vd_funcs.front();
  create_vd_funcs.pop();
  decoder_ = used_create_vd_func_(
      client_task_runner_, decoder_task_runner_,
      base::BindRepeating(&VideoDecoderPipeline::GetVideoFramePool,
                          base::Unretained(this)));
  if (!decoder_) {
    DVLOGF(2) << "Failed to create VideoDecoder.";
    used_create_vd_func_ = nullptr;
    return CreateAndInitializeVD(std::move(create_vd_funcs), config, low_delay,
                                 cdm_context, std::move(waiting_cb));
  }

  decoder_->Initialize(
      config, low_delay, cdm_context,
      base::BindOnce(&VideoDecoderPipeline::OnInitializeDone, weak_this_,
                     std::move(create_vd_funcs), config, low_delay, cdm_context,
                     waiting_cb),
      base::BindRepeating(&VideoDecoderPipeline::OnFrameDecodedThunk,
                          client_task_runner_, weak_this_),
      waiting_cb);
}

void VideoDecoderPipeline::OnInitializeDone(
    base::queue<VideoDecoderPipeline::CreateVDFunc> create_vd_funcs,
    VideoDecoderConfig config,
    bool low_delay,
    CdmContext* cdm_context,
    WaitingCB waiting_cb,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(init_cb_);
  DVLOGF(4) << "Initialization " << (success ? "success." : "failure.");

  if (success) {
    DVLOGF(2) << "Initialize VD successfully.";
    std::move(init_cb_).Run(true);
    return;
  }

  DVLOGF(3) << "Reset VD, try the next create function.";
  decoder_ = nullptr;
  used_create_vd_func_ = nullptr;
  CreateAndInitializeVD(std::move(create_vd_funcs), config, low_delay,
                        cdm_context, std::move(waiting_cb));
}

void VideoDecoderPipeline::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(decoder_);
  DCHECK(!client_reset_cb_);
  DVLOGF(3);

  client_reset_cb_ = std::move(closure);
  decoder_->Reset(
      base::BindOnce(&VideoDecoderPipeline::OnResetDone, weak_this_));
}

void VideoDecoderPipeline::OnResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_reset_cb_);
  DVLOGF(3);

  frame_converter_->AbortPendingFrames();

  CallFlushCbIfNeeded(DecodeStatus::ABORTED);

  std::move(client_reset_cb_).Run();
}

void VideoDecoderPipeline::Decode(scoped_refptr<DecoderBuffer> buffer,
                                  DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(decoder_);
  DVLOGF(4);

  bool is_flush = buffer->end_of_stream();
  decoder_->Decode(std::move(buffer),
                   base::BindOnce(&VideoDecoderPipeline::OnDecodeDone,
                                  weak_this_, is_flush, std::move(decode_cb)));
}

void VideoDecoderPipeline::OnDecodeDone(bool is_flush,
                                        DecodeCB decode_cb,
                                        DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(4) << "is_flush: " << is_flush << ", status: " << status;

  if (has_error_)
    status = DecodeStatus::DECODE_ERROR;

  if (is_flush && status == DecodeStatus::OK) {
    client_flush_cb_ = std::move(decode_cb);
    // TODO(akahuang): The order between flush cb and output cb is preserved
    // only when OnFrameDecodedThunk() run on |client_task_runner_|. Remove
    // OnFrameDecodedThunk() when we make sure all VD callbacks are called on
    // the same thread.
    CallFlushCbIfNeeded(DecodeStatus::OK);
    return;
  }

  std::move(decode_cb).Run(status);
}

// static
void VideoDecoderPipeline::OnFrameDecodedThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<base::WeakPtr<VideoDecoderPipeline>> pipeline,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(task_runner);
  DCHECK(pipeline);
  DVLOGF(4);

  // Workaround for some decoder's non-conformant behavior:
  // Decoders are supposed to call the output callback "as soon as possible",
  // i.e. directly in their own thread. If they do so the OnFrameDecoded task is
  // scheduled on the client task queue, and we have no race condition if we
  // are destroyed after that.
  //
  // But some decoders will run the output callback on their client thread, i.e.
  // our own task runner. If we get destroyed before that task is processed,
  // then OnFrameDecoded would be scheduled after our destruction and thus
  // would never be run, making the client miss a frame.
  //
  // So we first check whether we already are running on our task runner, and
  // execute OnFrameDecoded without delay in that case. Hopefully this can be
  // removed in the future.
  //
  // TODO fix the Mojo service so we don't need to do this dance anymore.
  if (task_runner->RunsTasksInCurrentSequence()) {
    if (*pipeline)
      (*pipeline)->OnFrameDecoded(std::move(frame));
    return;
  }

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&VideoDecoderPipeline::OnFrameDecoded,
                                       *pipeline, std::move(frame)));
}

void VideoDecoderPipeline::OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(frame_converter_);
  DVLOGF(4);

  frame_converter_->ConvertFrame(std::move(frame));
}

void VideoDecoderPipeline::OnFrameConverted(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(4);

  if (!frame)
    return OnError("Frame converter returns null frame.");
  if (has_error_) {
    DVLOGF(2) << "Skip returning frames after error occurs.";
    return;
  }

  client_output_cb_.Run(std::move(frame));

  // After outputting a frame, flush might be completed.
  CallFlushCbIfNeeded(DecodeStatus::OK);
}

void VideoDecoderPipeline::OnError(const std::string& msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  VLOGF(1) << msg;

  has_error_ = true;
  CallFlushCbIfNeeded(DecodeStatus::DECODE_ERROR);
}

void VideoDecoderPipeline::CallFlushCbIfNeeded(DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(3) << "status: " << status;

  if (!client_flush_cb_)
    return;

  // Flush is not completed yet.
  if (status == DecodeStatus::OK && frame_converter_->HasPendingFrames())
    return;

  std::move(client_flush_cb_).Run(status);
}

DmabufVideoFramePool* VideoDecoderPipeline::GetVideoFramePool() const {
  return frame_pool_.get();
}

}  // namespace media
