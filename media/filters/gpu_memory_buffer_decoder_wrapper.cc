// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_memory_buffer_decoder_wrapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"

namespace media {

GpuMemoryBufferDecoderWrapper::PendingDecode::PendingDecode(
    scoped_refptr<DecoderBuffer> buffer,
    DecodeCB decode_cb)
    : buffer(std::move(buffer)), decode_cb(std::move(decode_cb)) {}

GpuMemoryBufferDecoderWrapper::PendingDecode::PendingDecode(
    PendingDecode&& pending_decode) = default;

GpuMemoryBufferDecoderWrapper::PendingDecode::~PendingDecode() {
  if (decode_cb)
    decode_cb.Run(DecodeStatus::ABORTED);
}

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
  DCHECK_EQ(pending_copies_, 0u);
  DCHECK(pending_decodes_.empty());
  DCHECK(!eos_status_);

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
  DCHECK(!eos_status_);

  // It's now okay to start copying outputs again if Reset() disabled them.
  abort_copies_ = false;

  // Don't start too many decodes if copies are taking too long...
  pending_decodes_.emplace_back(buffer, decode_cb);
  MaybeStartNextDecode();
}

void GpuMemoryBufferDecoderWrapper::Reset(const base::Closure& reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Abort any in-flight copies and stop any new ones from being started until
  // the next Decode() call (which will not occur until Reset() completes).
  gmb_pool_->Abort();
  abort_copies_ = true;
  pending_copies_ = 0u;
  copy_factory_.InvalidateWeakPtrs();
  eos_status_.reset();

  // Abort any outstanding decodes. The order here is important so that we
  // return them in the scheduled order.
  active_decodes_.clear();
  pending_decodes_.clear();

  decoder_->Reset(reset_cb);
}

int GpuMemoryBufferDecoderWrapper::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return decoder_->GetMaxDecodeRequests();
}

void GpuMemoryBufferDecoderWrapper::MaybeStartNextDecode() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (pending_decodes_.empty())
    return;

  // Never allow more decodes than the decoder specifies. Callers are required
  // to guarantee that they never exceed GetMaxDecodeRequests().
  const size_t max_decodes = GetMaxDecodeRequests();
  DCHECK_LE(active_decodes_.size(), max_decodes);
  if (active_decodes_.size() == max_decodes)
    return;

  // Defer decoding if the number of pending copies exceeds the maximum number
  // of decodes; this ensures that if the decoder is fast but copies are slow,
  // we don't accumulate massive amounts of uncopied frames.
  //
  // E.g., when GetMaxDecodeRequests() == 1, this allows another decode if there
  // is at most 1 copy outstanding. When GMDR() == 2, this allows another decode
  // if there are at most 2 copies outstanding.
  if (pending_copies_ > max_decodes)
    return;

  active_decodes_.emplace_back(std::move(pending_decodes_.front()));
  pending_decodes_.pop_front();

  decoder_->Decode(
      active_decodes_.back().buffer,
      base::BindRepeating(&GpuMemoryBufferDecoderWrapper::OnDecodeComplete,
                          weak_factory_.GetWeakPtr()));
}

void GpuMemoryBufferDecoderWrapper::OnDecodeComplete(DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!eos_status_);

  // If Reset() was called after Decode() but before this callback, we should
  // do nothing. Our DecodeCB already returned ABORTED during Reset().
  if (abort_copies_) {
    DCHECK_EQ(pending_copies_, 0u);
    DCHECK(pending_decodes_.empty());
    DCHECK(active_decodes_.empty());
    return;
  }

  DCHECK(!active_decodes_.empty());

  // If this isn't an end of stream, return the decode status and try to
  // schedule another decode.
  if (!active_decodes_.front().buffer->end_of_stream()) {
    // Move the PendingDecode operation into a stack variable and start the next
    // decode before invoking the DecodeCB since that call may invoke other
    // operations on this class.
    PendingDecode pd(std::move(active_decodes_.front()));
    active_decodes_.pop_front();
    MaybeStartNextDecode();

    base::ResetAndReturn(&pd.decode_cb).Run(status);
    return;
  }

  // We must now be handling an end of stream.
  DCHECK(pending_decodes_.empty());
  DCHECK_EQ(active_decodes_.size(), 1u);

  // If all copies have finished we can return the status immediately. The
  // VideoDecoder API guarantees that this function won't be called until
  // _after_ all outputs have been sent to OnOutputReady().
  if (!pending_copies_) {
    // Move the PendingDecode operation into a stack variable since the call to
    // the DecodeCB may invoke other operations on this class.
    PendingDecode pd(std::move(active_decodes_.front()));
    active_decodes_.clear();
    base::ResetAndReturn(&pd.decode_cb).Run(status);
    return;
  }

  // Normal case, we have copies to wait for before issuing the DecodeCB. We
  // must ensure the callback is not fired until copies complete.
  eos_status_ = status;
}

void GpuMemoryBufferDecoderWrapper::OnOutputReady(
    const OutputCB& output_cb,
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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

  // If this is an end of stream, we've finished all pending copies and should
  // now notify the caller. Note: We must check |eos_status_| here because if
  // the DecodeCB hasn't returned, it's not safe to destroy the DecoderBuffer
  // it was given (since they are passed by const&...).
  if (!pending_copies_ && eos_status_) {
    DCHECK_EQ(active_decodes_.size(), 1u);
    DCHECK(active_decodes_.front().buffer->end_of_stream());

    // Move the PendingDecode operation and |eos_status_| values into stack
    // variables since the call to the DecodeCB may invoke other operations on
    // this class.
    PendingDecode pd(std::move(active_decodes_.front()));
    active_decodes_.clear();
    DecodeStatus status = *eos_status_;
    eos_status_.reset();

    base::ResetAndReturn(&pd.decode_cb).Run(status);
    return;
  }

  MaybeStartNextDecode();
}

}  // namespace media
