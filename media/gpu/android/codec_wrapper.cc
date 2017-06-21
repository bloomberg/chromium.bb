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

namespace media {

// CodecWrapperImpl is the implementation for CodecWrapper but is separate so
// we can keep its refcounting as an implementation detail. CodecWrapper and
// CodecOutputBuffer are the only two things that hold references to it.
class CodecWrapperImpl : public base::RefCountedThreadSafe<CodecWrapperImpl> {
 public:
  CodecWrapperImpl(std::unique_ptr<MediaCodecBridge> codec);

  std::unique_ptr<MediaCodecBridge> TakeCodec();
  bool HasValidCodecOutputBuffers() const;
  void DiscardCodecOutputBuffers();
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
  MediaCodecStatus DequeueInputBuffer(base::TimeDelta timeout, int* index);
  MediaCodecStatus DequeueOutputBuffer(
      base::TimeDelta timeout,
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);
  bool SetSurface(jobject surface);

  // Releases the codec buffer and optionally renders it. This is a noop if
  // the codec buffer is not valid (i.e., there's no race between checking its
  // validity and releasing it). Can be called on any thread. Returns true if
  // the buffer was released.
  bool ReleaseCodecOutputBuffer(int64_t id, bool render);

 private:
  friend base::RefCountedThreadSafe<CodecWrapperImpl>;
  ~CodecWrapperImpl();

  void DiscardCodecOutputBuffers_Locked();

  // |lock_| protects access to all member variables.
  mutable base::Lock lock_;
  std::unique_ptr<MediaCodecBridge> codec_;
  bool in_error_state_;

  // Buffer ids are unique for a given CodecWrapper and map to MediaCodec buffer
  // indices.
  int64_t next_buffer_id_;
  base::flat_map<int64_t, int> buffer_ids_;

  // The current output size. Updated when DequeueOutputBuffer() reports
  // OUTPUT_FORMAT_CHANGED.
  gfx::Size size_;

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

CodecWrapperImpl::CodecWrapperImpl(std::unique_ptr<MediaCodecBridge> codec)
    : codec_(std::move(codec)), in_error_state_(false), next_buffer_id_(0) {}

CodecWrapperImpl::~CodecWrapperImpl() = default;

std::unique_ptr<MediaCodecBridge> CodecWrapperImpl::TakeCodec() {
  base::AutoLock l(lock_);
  if (!codec_)
    return nullptr;
  DiscardCodecOutputBuffers_Locked();
  return std::move(codec_);
}

bool CodecWrapperImpl::HasValidCodecOutputBuffers() const {
  base::AutoLock l(lock_);
  return !buffer_ids_.empty();
}

void CodecWrapperImpl::DiscardCodecOutputBuffers() {
  base::AutoLock l(lock_);
  DiscardCodecOutputBuffers_Locked();
}

void CodecWrapperImpl::DiscardCodecOutputBuffers_Locked() {
  lock_.AssertAcquired();
  for (auto& kv : buffer_ids_)
    codec_->ReleaseOutputBuffer(kv.second, false);
  buffer_ids_.clear();
}

bool CodecWrapperImpl::SupportsFlush(DeviceInfo* device_info) const {
  base::AutoLock l(lock_);
  return !device_info->CodecNeedsFlushWorkaround(codec_.get());
}

bool CodecWrapperImpl::Flush() {
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);

  // Dequeued output buffers are invalidated by flushing.
  buffer_ids_.clear();
  auto status = codec_->Flush();
  if (status == MEDIA_CODEC_ERROR) {
    in_error_state_ = true;
    return false;
  }
  return true;
}

MediaCodecStatus CodecWrapperImpl::QueueInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    base::TimeDelta presentation_time) {
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);

  auto status =
      codec_->QueueInputBuffer(index, data, data_size, presentation_time);
  if (status == MEDIA_CODEC_ERROR)
    in_error_state_ = true;
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
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);

  auto status = codec_->QueueSecureInputBuffer(
      index, data, data_size, key_id, iv, subsamples, encryption_scheme,
      presentation_time);
  if (status == MEDIA_CODEC_ERROR)
    in_error_state_ = true;
  return status;
}

void CodecWrapperImpl::QueueEOS(int input_buffer_index) {
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);
  codec_->QueueEOS(input_buffer_index);
}

MediaCodecStatus CodecWrapperImpl::DequeueInputBuffer(base::TimeDelta timeout,
                                                      int* index) {
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);
  auto status = codec_->DequeueInputBuffer(timeout, index);
  if (status == MEDIA_CODEC_ERROR)
    in_error_state_ = true;
  return status;
}

MediaCodecStatus CodecWrapperImpl::DequeueOutputBuffer(
    base::TimeDelta timeout,
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);
  // If |*codec_buffer| were not null, deleting it may deadlock when it
  // tries to release itself.
  DCHECK(!*codec_buffer);

  // Dequeue in a loop so we can avoid propagating the uninteresting
  // OUTPUT_FORMAT_CHANGED and OUTPUT_BUFFERS_CHANGED statuses to our caller.
  for (int attempt = 0; attempt < 3; ++attempt) {
    int index = -1;
    size_t unused_offset = 0;
    size_t unused_size = 0;
    bool* unused_key_frame = nullptr;
    auto status = codec_->DequeueOutputBuffer(timeout, &index, &unused_offset,
                                              &unused_size, presentation_time,
                                              end_of_stream, unused_key_frame);
    switch (status) {
      case MEDIA_CODEC_OK: {
        int64_t buffer_id = next_buffer_id_++;
        buffer_ids_[buffer_id] = index;
        *codec_buffer =
            base::WrapUnique(new CodecOutputBuffer(this, buffer_id, size_));
        return status;
      }
      case MEDIA_CODEC_ERROR: {
        in_error_state_ = true;
        return status;
      }
      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        // An OUTPUT_FORMAT_CHANGED is not reported after Flush() if the frame
        // size does not change.
        if (codec_->GetOutputSize(&size_) == MEDIA_CODEC_ERROR) {
          in_error_state_ = true;
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

  in_error_state_ = true;
  return MEDIA_CODEC_ERROR;
}

bool CodecWrapperImpl::SetSurface(jobject surface) {
  base::AutoLock l(lock_);
  DCHECK(codec_ && !in_error_state_);

  bool status = codec_->SetSurface(surface);
  if (!status)
    in_error_state_ = true;
  return status;
}

bool CodecWrapperImpl::ReleaseCodecOutputBuffer(int64_t id, bool render) {
  base::AutoLock l(lock_);
  if (!codec_ || in_error_state_)
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
  return true;
}

CodecWrapper::CodecWrapper(std::unique_ptr<MediaCodecBridge> codec)
    : impl_(new CodecWrapperImpl(std::move(codec))) {}

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

MediaCodecStatus CodecWrapper::DequeueInputBuffer(base::TimeDelta timeout,
                                                  int* index) {
  return impl_->DequeueInputBuffer(timeout, index);
}

MediaCodecStatus CodecWrapper::DequeueOutputBuffer(
    base::TimeDelta timeout,
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  return impl_->DequeueOutputBuffer(timeout, presentation_time, end_of_stream,
                                    codec_buffer);
}

bool CodecWrapper::SetSurface(jobject surface) {
  return impl_->SetSurface(surface);
}

}  // namespace media
