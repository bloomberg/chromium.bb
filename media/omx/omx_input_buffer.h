// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Defines the input buffer object for the video decoder. This provides
// the interface needed by the video decoder to read input data.
//
// This object is implemened using system memory.

#ifndef MEDIA_OMX_OMX_INPUT_BUFFER_H_
#define MEDIA_OMX_OMX_INPUT_BUFFER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "media/base/buffers.h"

namespace media {

class OmxInputBuffer : public Buffer {
 public:
  // Creates an empty input buffer.
  OmxInputBuffer();

  // Creates an input buffer given |data| and |size|.
  // After construction, this object will be given the ownership of
  // |data| and is responsible for deleting it.
  OmxInputBuffer(uint8* data, int size);
  virtual ~OmxInputBuffer();

  // Read from the this buffer into |data| with the maximum |size| bytes.
  // Returns number of bytes read. If a read is successful, the number
  // of used bytes will advances accordingly.
  // Returns a negative number on error.
  int Read(uint8* data, int size);

  // Returns true if this buffer is used.
  bool Used();

 private:
  virtual const uint8* GetData() const { return data_.get(); }

  virtual size_t GetDataSize() const { return size_; }

  scoped_array<uint8> data_;
  int size_;
  int used_;

  DISALLOW_COPY_AND_ASSIGN(OmxInputBuffer);
};

}  // namespace media

#endif  // MEDIA_OMX_OMX_INPUT_BUFFER_H_
