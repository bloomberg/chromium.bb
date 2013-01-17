// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include "base/logging.h"
#include "media/base/decrypt_config.h"

namespace media {

DecoderBuffer::DecoderBuffer(int size)
    : size_(size) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8* data, int size)
    : size_(size) {
  if (!data) {
    CHECK_EQ(size_, 0);
    return;
  }

  Initialize();
  memcpy(data_.get(), data, size_);
}

DecoderBuffer::~DecoderBuffer() {}

void DecoderBuffer::Initialize() {
  CHECK_GE(size_, 0);
  data_.reset(reinterpret_cast<uint8*>(
      base::AlignedAlloc(size_ + kPaddingSize, kAlignmentSize)));
  memset(data_.get() + size_, 0, kPaddingSize);
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8* data,
                                                     int data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new DecoderBuffer(NULL, 0));
}

base::TimeDelta DecoderBuffer::GetTimestamp() const {
  return timestamp_;
}

void DecoderBuffer::SetTimestamp(const base::TimeDelta& timestamp) {
  timestamp_ = timestamp;
}

base::TimeDelta DecoderBuffer::GetDuration() const {
  return duration_;
}

void DecoderBuffer::SetDuration(const base::TimeDelta& duration) {
  duration_ = duration;
}

const uint8* DecoderBuffer::GetData() const {
  return data_.get();
}

uint8* DecoderBuffer::GetWritableData() const {
  return data_.get();
}

int DecoderBuffer::GetDataSize() const {
  return size_;
}

const DecryptConfig* DecoderBuffer::GetDecryptConfig() const {
  return decrypt_config_.get();
}

void DecoderBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  decrypt_config_ = decrypt_config.Pass();
}

bool DecoderBuffer::IsEndOfStream() const {
  return data_ == NULL;
}

}  // namespace media
