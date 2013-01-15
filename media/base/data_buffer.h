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
  // Assumes valid data of size |buffer_size|.
  DataBuffer(scoped_array<uint8> buffer, int buffer_size);

  // Allocates buffer of size |buffer_size|. If |buffer_size| is 0, |data_| is
  // set to NULL and this becomes an end of stream buffer.
  //
  // TODO(scherkus): Enforce calling CreateEOSBuffer() instead of passing 0 and
  // sprinkle DCHECK()s everywhere.
  explicit DataBuffer(int buffer_size);

  // Allocates buffer of size |data_size|, copies [data,data+data_size) to
  // the allocated buffer and sets data size to |data_size|.
  DataBuffer(const uint8* data, int data_size);

  base::TimeDelta GetTimestamp() const;
  void SetTimestamp(const base::TimeDelta& timestamp);

  base::TimeDelta GetDuration() const;
  void SetDuration(const base::TimeDelta& duration);

  const uint8* GetData() const;
  uint8* GetWritableData();

  // The size of valid data in bytes, which must be less than or equal
  // to GetBufferSize().
  int GetDataSize() const;
  void SetDataSize(int data_size);

  // Returns the size of the underlying buffer.
  int GetBufferSize() const;

  // If there's no data in this buffer, it represents end of stream.
  bool IsEndOfStream() const;

 protected:
  friend class base::RefCountedThreadSafe<DataBuffer>;
  virtual ~DataBuffer();

 private:
  // Constructor helper method for memory allocations.
  void Initialize();

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  scoped_array<uint8> data_;
  int buffer_size_;
  int data_size_;

  DISALLOW_COPY_AND_ASSIGN(DataBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
