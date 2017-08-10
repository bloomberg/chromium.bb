// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bind_to_current_loop.h"

namespace media {

// CodecWrapperImpl is the implementation for CodecWrapper but is separate so
// we can keep its refcounting as an implementation detail. CodecWrapper and
// CodecOutputBuffer are the only two things that hold references to it.
class CodecWrapperImpl : public base::RefCountedThreadSafe<CodecWrapperImpl> {
 public:
  CodecWrapperImpl(std::unique_ptr<MediaCodecBridge> codec,
                   base::Closure output_buffer_release_cb);

  std::unique_ptr<MediaCodecBridge> TakeCodec();
  bool HasValidCodecOutputBuffers() const;
  void DiscardCodecOutputBuffers();
  bool IsFlushed() const;
  bool IsDraining() const;
  bool IsDrained() const;
  bool SupportsFlush(DeviceInfo* device_info) const;
  bool Flush();
  MediaCodecStatus QueueInputBuffer(int index,
                                    const uint8_t* data,
                                    size_t data_size,
                                    base::TimeDelta presentation_time);
  MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      const EncryptionScheme& encryption_scheme,
      base::TimeDelta presentation_time);
  void QueueEOS(int input_buffer_index);
  MediaCodecStatus DequeueInputBuffer(int* index);
  MediaCodecStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);
  MediaCodecStatus SetSurface(const base::android::JavaRef<jobject>& surface);

  // Releases the codec buffer and optionally renders it. This is a noop if
  // the codec buffer is not valid. Can be called on any thread. Returns true if
  // the buffer was released.
  bool ReleaseCodecOutputBuffer(int64_t id, bool render);

 private:
  enum class State {
    kError,
    kFlushed,
    kRunning,
    kDraining,
    kDrained,
  };

  friend base::RefCountedThreadSafe<CodecWrapperImpl>;
  ~CodecWrapperImpl();

  void DiscardCodecOutputBuffers_Locked();

  // |lock_| protects access to all member variables.
  mutable base::Lock lock_;
  std::unique_ptr<MediaCodecBridge> codec_;
  State state_;

  // Buffer ids are unique for a given CodecWrapper and map to MediaCodec buffer
  // indices.
  int64_t next_buffer_id_;
  base::flat_map<int64_t, int> buffer_ids_;

  // The current output size. Updated when DequeueOutputBuffer() reports
  // OUTPUT_FORMAT_CHANGED.
  gfx::Size size_;

  // A callback that's called whenever an output buffer is released back to the
  // codec.
  base::Closure output_buffer_release_cb_;

  DISALLOW_COPY_AND_ASSIGN(CodecWrapperImpl);
};

CodecOutputBuffer::CodecOutputBuffer(scoped_refptr<CodecWrapperImpl> codec,
                                     int64_t id,
                                     gfx::Size size)
    : codec_(std::move(codec)), id_(id), size_(size) {}

CodecOutputBuffer::~CodecOutputBuffer() {
  codec_->ReleaseCodecOutputBuffer(id_, false);
}

bool CodecOutputBuffer::ReleaseToSurface() {
  return codec_->ReleaseCodecOutputBuffer(id_, true);
}

CodecWrapperImpl::CodecWrapperImpl(std::unique_ptr<MediaCodecBridge> codec,
                                   base::Closure output_buffer_release_cb)
    : codec_(std::move(codec)),
      state_(State::kFlushed),
      next_buffer_id_(0),
      output_buffer_release_cb_(std::move(output_buffer_release_cb)) {
  DVLOG(2) << __func__;
}

CodecWrapperImpl::~CodecWrapperImpl() = default;

std::unique_ptr<MediaCodecBridge> CodecWrapperImpl::TakeCodec() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  if (!codec_)
    return nullptr;
  DiscardCodecOutputBuffers_Locked();
  return std::move(codec_);
}

bool CodecWrapperImpl::IsFlushed() const {
  base::AutoLock l(lock_);
  return state_ == State::kFlushed;
}

bool CodecWrapperImpl::IsDraining() const {
  base::AutoLock l(lock_);
  return state_ == State::kDraining;
}

bool CodecWrapperImpl::IsDrained() const {
  base::AutoLock l(lock_);
  return state_ == State::kDrained;
}

bool CodecWrapperImpl::HasValidCodecOutputBuffers() const {
  base::AutoLock l(lock_);
  return !buffer_ids_.empty();
}

void CodecWrapperImpl::DiscardCodecOutputBuffers() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DiscardCodecOutputBuffers_Locked();
}

void CodecWrapperImpl::DiscardCodecOutputBuffers_Locked() {
  DVLOG(2) << __func__;
  lock_.AssertAcquired();
  for (auto& kv : buffer_ids_)
    codec_->ReleaseOutputBuffer(kv.second, false);
  buffer_ids_.clear();
}

bool CodecWrapperImpl::SupportsFlush(DeviceInfo* device_info) const {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  return !device_info->CodecNeedsFlushWorkaround(codec_.get());
}

bool CodecWrapperImpl::Flush() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  // Dequeued output buffers are invalidated by flushing.
  buffer_ids_.clear();
  auto status = codec_->Flush();
  if (status == MEDIA_CODEC_ERROR) {
    state_ = State::kError;
    return false;
  }
  state_ = State::kFlushed;
  return true;
}

MediaCodecStatus CodecWrapperImpl::QueueInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    base::TimeDelta presentation_time) {
  DVLOG(4) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  auto status =
      codec_->QueueInputBuffer(index, data, data_size, presentation_time);
  if (status == MEDIA_CODEC_ERROR)
    state_ = State::kError;
  else
    state_ = State::kRunning;
  return status;
}

MediaCodecStatus CodecWrapperImpl::QueueSecureInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples,
    const EncryptionScheme& encryption_scheme,
    base::TimeDelta presentation_time) {
  DVLOG(4) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  auto status = codec_->QueueSecureInputBuffer(
      index, data, data_size, key_id, iv, subsamples, encryption_scheme,
      presentation_time);
  if (status == MEDIA_CODEC_ERROR)
    state_ = State::kError;
  else
    state_ = State::kRunning;
  return status;
}

void CodecWrapperImpl::QueueEOS(int input_buffer_index) {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);
  // Some MediaCodecs consider it an error to get an EOS as the first buffer
  // (http://crbug.com/672268).
  DCHECK_NE(state_, State::kFlushed);
  codec_->QueueEOS(input_buffer_index);
  state_ = State::kDraining;
}

MediaCodecStatus CodecWrapperImpl::DequeueInputBuffer(int* index) {
  DVLOG(4) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);
  auto status = codec_->DequeueInputBuffer(base::TimeDelta(), index);
  if (status == MEDIA_CODEC_ERROR)
    state_ = State::kError;
  return status;
}

MediaCodecStatus CodecWrapperImpl::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  DVLOG(4) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);
  // If |*codec_buffer| were not null, deleting it may deadlock when it
  // tries to release itself.
  DCHECK(!*codec_buffer);

  // Dequeue in a loop so we can avoid propagating the uninteresting
  // OUTPUT_FORMAT_CHANGED and OUTPUT_BUFFERS_CHANGED statuses to our caller.
  for (int attempt = 0; attempt < 3; ++attempt) {
    int index = -1;
    size_t unused;
    bool eos = false;
    auto status =
        codec_->DequeueOutputBuffer(base::TimeDelta(), &index, &unused, &unused,
                                    presentation_time, &eos, nullptr);
    switch (status) {
      case MEDIA_CODEC_OK: {
        if (eos) {
          state_ = State::kDrained;
          // We assume that the EOS flag is only ever attached to empty output
          // buffers because we submit the EOS flag on empty input buffers. The
          // MediaCodec docs leave open the possibility that the last non-empty
          // output buffer has the EOS flag but we haven't seen that happen.
          codec_->ReleaseOutputBuffer(index, false);
          if (end_of_stream)
            *end_of_stream = true;
          return status;
        }

        int64_t buffer_id = next_buffer_id_++;
        buffer_ids_[buffer_id] = index;
        *codec_buffer =
            base::WrapUnique(new CodecOutputBuffer(this, buffer_id, size_));
        return status;
      }
      case MEDIA_CODEC_ERROR: {
        state_ = State::kError;
        return status;
      }
      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        // An OUTPUT_FORMAT_CHANGED is not reported after Flush() if the frame
        // size does not change.
        if (codec_->GetOutputSize(&size_) == MEDIA_CODEC_ERROR) {
          state_ = State::kError;
          return MEDIA_CODEC_ERROR;
        }
        continue;
      }
      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        continue;
      default:
        return status;
    }
  }

  state_ = State::kError;
  return MEDIA_CODEC_ERROR;
}

MediaCodecStatus CodecWrapperImpl::SetSurface(
    const base::android::JavaRef<jobject>& surface) {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  if (!codec_->SetSurface(surface)) {
    state_ = State::kError;
    return MEDIA_CODEC_ERROR;
  }
  return MEDIA_CODEC_OK;
}

bool CodecWrapperImpl::ReleaseCodecOutputBuffer(int64_t id, bool render) {
  DVLOG(2) << __func__ << " render: " << render << ", id: " << id;
  base::AutoLock l(lock_);
  if (!codec_ || state_ == State::kError)
    return false;

  auto buffer_it = buffer_ids_.find(id);
  if (buffer_it == buffer_ids_.end())
    return false;

  // Discard the buffers preceding the one we're releasing.
  for (auto it = buffer_ids_.begin(); it < buffer_it; ++it) {
    int index = it->second;
    codec_->ReleaseOutputBuffer(index, false);
  }
  int index = buffer_it->second;
  codec_->ReleaseOutputBuffer(index, render);
  buffer_ids_.erase(buffer_ids_.begin(), buffer_it + 1);
  if (output_buffer_release_cb_)
    output_buffer_release_cb_.Run();
  return true;
}

CodecWrapper::CodecWrapper(std::unique_ptr<MediaCodecBridge> codec,
                           base::Closure output_buffer_release_cb)
    : impl_(new CodecWrapperImpl(std::move(codec),
                                 std::move(output_buffer_release_cb))) {}

CodecWrapper::~CodecWrapper() {
  // The codec must have already been taken.
  DCHECK(!impl_->TakeCodec());
}

std::unique_ptr<MediaCodecBridge> CodecWrapper::TakeCodec() {
  return impl_->TakeCodec();
}

bool CodecWrapper::HasValidCodecOutputBuffers() const {
  return impl_->HasValidCodecOutputBuffers();
}

void CodecWrapper::DiscardCodecOutputBuffers() {
  impl_->DiscardCodecOutputBuffers();
}

bool CodecWrapper::SupportsFlush(DeviceInfo* device_info) const {
  return impl_->SupportsFlush(device_info);
}

bool CodecWrapper::IsFlushed() const {
  return impl_->IsFlushed();
}

bool CodecWrapper::IsDraining() const {
  return impl_->IsDraining();
}

bool CodecWrapper::IsDrained() const {
  return impl_->IsDrained();
}

bool CodecWrapper::Flush() {
  return impl_->Flush();
}

MediaCodecStatus CodecWrapper::QueueInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    base::TimeDelta presentation_time) {
  return impl_->QueueInputBuffer(index, data, data_size, presentation_time);
}

MediaCodecStatus CodecWrapper::QueueSecureInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples,
    const EncryptionScheme& encryption_scheme,
    base::TimeDelta presentation_time) {
  return impl_->QueueSecureInputBuffer(index, data, data_size, key_id, iv,
                                       subsamples, encryption_scheme,
                                       presentation_time);
}

void CodecWrapper::QueueEOS(int input_buffer_index) {
  impl_->QueueEOS(input_buffer_index);
}

MediaCodecStatus CodecWrapper::DequeueInputBuffer(int* index) {
  return impl_->DequeueInputBuffer(index);
}

MediaCodecStatus CodecWrapper::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  return impl_->DequeueOutputBuffer(presentation_time, end_of_stream,
                                    codec_buffer);
}

MediaCodecStatus CodecWrapper::SetSurface(
    const base::android::JavaRef<jobject>& surface) {
  return impl_->SetSurface(surface);
}

}  // namespace media
