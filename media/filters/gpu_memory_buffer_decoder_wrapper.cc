// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_memory_buffer_decoder_wrapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "media/base/decoder_buffer.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"

namespace media {

GpuMemoryBufferDecoderWrapper::GpuMemoryBufferDecoderWrapper(
    std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool,
    std::unique_ptr<VideoDecoder> decoder)
    : gmb_pool_(std::move(gmb_pool)),
      decoder_(std::move(decoder)),
      weak_factory_(this),
      copy_factory_(this) {
  DCHECK(gmb_pool_);
  DCHECK(decoder_);
  DETACH_FROM_THREAD(thread_checker_);
}

GpuMemoryBufferDecoderWrapper::~GpuMemoryBufferDecoderWrapper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::string GpuMemoryBufferDecoderWrapper::GetDisplayName() const {
  // This call is expected to be static and safe to call from any thread.
  return decoder_->GetDisplayName();
}

void GpuMemoryBufferDecoderWrapper::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(eos_decode_cb_.is_null());
  DCHECK_EQ(pending_copies_, 0u);

  // Follow the standard Initialize() path, but replace the OutputCB provided
  // with our own which handles coping to the GpuMemoryBuffer.
  decoder_->Initialize(
      config, low_delay, cdm_context, init_cb,
      base::BindRepeating(&GpuMemoryBufferDecoderWrapper::OnOutputReady,
                          weak_factory_.GetWeakPtr(), output_cb),
      waiting_for_decryption_key_cb);
}

void GpuMemoryBufferDecoderWrapper::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decode_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(eos_decode_cb_.is_null());

  // It's now okay to start copying outputs again if Reset() disabled them.
  abort_copies_ = false;

  // End of stream is a special case where we need to intercept the DecodeCB and
  // wait until all copies are done before triggering it.
  if (buffer->end_of_stream()) {
    decoder_->Decode(buffer, base::BindRepeating(
                                 &GpuMemoryBufferDecoderWrapper::OnDecodedEOS,
                                 weak_factory_.GetWeakPtr(), decode_cb));
    return;
  }

  decoder_->Decode(buffer, decode_cb);
}

void GpuMemoryBufferDecoderWrapper::Reset(const base::Closure& reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Abort any in-flight copies and stop any new ones from being started until
  // the next Decode() call (which will not occur until Reset() completes).
  gmb_pool_->Abort();
  abort_copies_ = true;
  pending_copies_ = 0u;
  copy_factory_.InvalidateWeakPtrs();

  // In case OnDecodedEOS() was called, but we have pending copies, we need to
  // issue the |eos_decode_cb_| here since we're aborting any subsequent copies
  // or OnOutputReady calls.
  //
  // Technically the status here should be ABORTED, but it doesn't matter and
  // avoids adding yet another state variable to this already completed process.
  if (!eos_decode_cb_.is_null())
    std::move(eos_decode_cb_).Run();

  decoder_->Reset(reset_cb);
}

int GpuMemoryBufferDecoderWrapper::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return decoder_->GetMaxDecodeRequests();
}

void GpuMemoryBufferDecoderWrapper::OnDecodedEOS(const DecodeCB& decode_cb,
                                                 DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(eos_decode_cb_.is_null());

  // If Reset() was called after Decode() but before this callback, we should
  // return ABORTED for the decode callback. Any pending outputs or copies will
  // have been aborted by Reset().
  if (abort_copies_) {
    DCHECK_EQ(pending_copies_, 0u);
    decode_cb.Run(DecodeStatus::ABORTED);
    return;
  }

  // If all copies have finished we can return the status immediately. The
  // VideoDecoder API guarantees that this function won't be called until
  // _after_ all outputs have been sent to OnOutputReady().
  if (!pending_copies_) {
    decode_cb.Run(status);
    return;
  }

  // Normal case, we have copies to wait for before issuing the DecodeCB. We
  // must ensure this callback is not fired until copies complete.
  //
  // It's crucial we set |eos_decode_cb_| only after signaled by the underlying
  // decoder and not during Decode() itself, otherwise it's possible that
  // |pending_copies_| will reach zero for previously started copies and notify
  // the end of stream before we actually vend all frames.
  eos_decode_cb_ = base::BindOnce(decode_cb, status);
}

void GpuMemoryBufferDecoderWrapper::OnOutputReady(
    const OutputCB& output_cb,
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This should not be set until after all outputs have been queued.
  DCHECK(eos_decode_cb_.is_null());

  // If Reset() has been called, we shouldn't waste cycles on useless copies.
  if (abort_copies_)
    return;

  ++pending_copies_;
  gmb_pool_->MaybeCreateHardwareFrame(
      frame, base::BindOnce(&GpuMemoryBufferDecoderWrapper::OnFrameCopied,
                            copy_factory_.GetWeakPtr(), output_cb));
}

void GpuMemoryBufferDecoderWrapper::OnFrameCopied(
    const OutputCB& output_cb,
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(pending_copies_, 0u);
  --pending_copies_;

  output_cb.Run(frame);

  // We've finished all pending copies and should now notify the caller.
  if (!pending_copies_ && !eos_decode_cb_.is_null())
    std::move(eos_decode_cb_).Run();
}

}  // namespace media
