// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include "base/logging.h"
#include "media/base/decrypt_config.h"

namespace media {

DecoderBuffer::DecoderBuffer(int size)
    : size_(size),
      side_data_size_(0) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8* data, int size)
    : size_(size),
      side_data_size_(0) {
  if (!data) {
    CHECK_EQ(size_, 0);
    return;
  }

  Initialize();
  memcpy(data_.get(), data, size_);
}

DecoderBuffer::DecoderBuffer(const uint8* data, int size,
                             const uint8* side_data, int side_data_size)
    : size_(size),
      side_data_size_(side_data_size) {
  if (!data) {
    CHECK_EQ(size_, 0);
    return;
  }

  Initialize();
  memcpy(data_.get(), data, size_);
  memcpy(side_data_.get(), side_data, side_data_size_);
}

DecoderBuffer::~DecoderBuffer() {}

void DecoderBuffer::Initialize() {
  CHECK_GE(size_, 0);
  data_.reset(reinterpret_cast<uint8*>(
      base::AlignedAlloc(size_ + kPaddingSize, kAlignmentSize)));
  memset(data_.get() + size_, 0, kPaddingSize);
  if (side_data_size_ > 0) {
    side_data_.reset(reinterpret_cast<uint8*>(
        base::AlignedAlloc(side_data_size_ + kPaddingSize, kAlignmentSize)));
    memset(side_data_.get() + side_data_size_, 0, kPaddingSize);
  }
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8* data,
                                                     int data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8* data,
                                                     int data_size,
                                                     const uint8* side_data,
                                                     int side_data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  CHECK(side_data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size,
                                              side_data, side_data_size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new DecoderBuffer(NULL, 0));
}

base::TimeDelta DecoderBuffer::GetTimestamp() const {
  DCHECK(!IsEndOfStream());
  return timestamp_;
}

void DecoderBuffer::SetTimestamp(const base::TimeDelta& timestamp) {
  DCHECK(!IsEndOfStream());
  timestamp_ = timestamp;
}

base::TimeDelta DecoderBuffer::GetDuration() const {
  DCHECK(!IsEndOfStream());
  return duration_;
}

void DecoderBuffer::SetDuration(const base::TimeDelta& duration) {
  DCHECK(!IsEndOfStream());
  duration_ = duration;
}

const uint8* DecoderBuffer::GetData() const {
  DCHECK(!IsEndOfStream());
  return data_.get();
}

uint8* DecoderBuffer::GetWritableData() const {
  DCHECK(!IsEndOfStream());
  return data_.get();
}

int DecoderBuffer::GetDataSize() const {
  DCHECK(!IsEndOfStream());
  return size_;
}

const uint8* DecoderBuffer::GetSideData() const {
  DCHECK(!IsEndOfStream());
  return side_data_.get();
}

int DecoderBuffer::GetSideDataSize() const {
  DCHECK(!IsEndOfStream());
  return side_data_size_;
}

const DecryptConfig* DecoderBuffer::GetDecryptConfig() const {
  DCHECK(!IsEndOfStream());
  return decrypt_config_.get();
}

void DecoderBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  DCHECK(!IsEndOfStream());
  decrypt_config_ = decrypt_config.Pass();
}

bool DecoderBuffer::IsEndOfStream() const {
  return data_ == NULL;
}

std::string DecoderBuffer::AsHumanReadableString() {
  if (IsEndOfStream()) {
    return "end of stream";
  }

  std::ostringstream s;
  s << "timestamp: " << timestamp_.InMicroseconds()
    << " duration: " << duration_.InMicroseconds()
    << " size: " << size_
    << " side_data_size: " << side_data_size_
    << " encrypted: " << (decrypt_config_ != NULL);
  return s.str();
}

}  // namespace media
