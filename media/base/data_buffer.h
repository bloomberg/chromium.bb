// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

// A simple buffer that takes ownership of the given data pointer or allocates
// as necessary.
//
// Unlike DecoderBuffer, allocations are assumed to be allocated with the
// default memory allocator (i.e., new uint8[]).
class MEDIA_EXPORT DataBuffer : public base::RefCountedThreadSafe<DataBuffer> {
 public:
  // Allocates buffer of size |buffer_size| >= 0.
  explicit DataBuffer(int buffer_size);

  // Assumes valid data of size |buffer_size|.
  DataBuffer(scoped_array<uint8> buffer, int buffer_size);

  // Create a DataBuffer whose |data_| is copied from |data|.
  //
  // |data| must not be null and |size| must be >= 0.
  static scoped_refptr<DataBuffer> CopyFrom(const uint8* data, int size);

  // Create a DataBuffer indicating we've reached end of stream.
  //
  // Calling any method other than IsEndOfStream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DataBuffer> CreateEOSBuffer();

  base::TimeDelta GetTimestamp() const;
  void SetTimestamp(const base::TimeDelta& timestamp);

  base::TimeDelta GetDuration() const;
  void SetDuration(const base::TimeDelta& duration);

  const uint8* GetData() const;
  uint8* GetWritableData();

  // The size of valid data in bytes.
  //
  // Setting this value beyond the buffer size is disallowed.
  int GetDataSize() const;
  void SetDataSize(int data_size);

  // If there's no data in this buffer, it represents end of stream.
  bool IsEndOfStream() const;

 protected:
  friend class base::RefCountedThreadSafe<DataBuffer>;

  // Allocates buffer of size |data_size|, copies [data,data+data_size) to
  // the allocated buffer and sets data size to |data_size|.
  //
  // If |data| is null an end of stream buffer is created.
  DataBuffer(const uint8* data, int data_size);

  virtual ~DataBuffer();

 private:
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  scoped_array<uint8> data_;
  int buffer_size_;
  int data_size_;

  DISALLOW_COPY_AND_ASSIGN(DataBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
