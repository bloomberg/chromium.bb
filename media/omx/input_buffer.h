// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Defines the input buffer object for the video decoder. This provides
// the interface needed by the video decoder to read input data.
//
// This object is implemened using system memory.

#include "base/basictypes.h"

namespace media {

class InputBuffer {
 public:
  // Creates an empty input buffer.
  InputBuffer();

  // Creates an input buffer given |data| and |size|.
  // After construction, this object will be given the ownership of
  // |data| and is responsible for deleting it.
  InputBuffer(uint8* data, int size);
  virtual ~InputBuffer();

  // Read from the this buffer into |data| with the maximum |size| bytes.
  // Returns number of bytes read. If a read is successful, the number
  // of used bytes will advances accordingly.
  // Returns a negative number on error.
  virtual int Read(uint8* data, int size);

  // Returns true if this buffer is used.
  virtual bool Used();

  // Returns true if this is an end-of-stream buffer.
  virtual bool Eos();

 private:
  uint8* data_;
  int size_;
  int used_;
};

}  // namespace media
