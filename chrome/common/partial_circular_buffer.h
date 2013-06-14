// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PARTIAL_CIRCULAR_BUFFER_H_
#define CHROME_COMMON_PARTIAL_CIRCULAR_BUFFER_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"

// A wrapper around a memory buffer that allows circular read and write with a
// selectable wrapping position. Buffer layout (after wrap; H is header):
// -----------------------------------------------------------
// | H | Beginning           | End           | Middle        |
// -----------------------------------------------------------
//  ^---- Non-wrapping -----^ ^--------- Wrapping ----------^
// The non-wrapping part is never overwritten. The wrapping part will be
// circular. The very first part is the header (see the BufferData struct
// below). It consists of the following information:
// - Length written to the buffer (not including header).
// - Wrapping position.
// - End position of buffer. (If the last byte is at x, this will be x + 1.)
// Users of wrappers around the same underlying buffer must ensure that writing
// is finished before reading is started.
class PartialCircularBuffer {
 public:
  // Use for reading. |buffer_size| is in bytes and must be larger than the
  // header size (see above).
  PartialCircularBuffer(void* buffer, uint32 buffer_size);

  // Use for writing. |buffer_size| is in bytes and must be larger than the
  // header size (see above). If |append| is true, the header data is not reset
  // and writing will continue were left off, |wrap_position| is then ignored.
  PartialCircularBuffer(void* buffer,
                        uint32 buffer_size,
                        uint32 wrap_position,
                        bool append);

  uint32 Read(void* buffer, uint32 buffer_size);
  void Write(const void* buffer, uint32 buffer_size);

 private:
  friend class PartialCircularBufferTest;

#pragma pack(push)
#pragma pack(4)
  struct BufferData {
    uint32 total_written;
    uint32 wrap_position;
    uint32 end_position;
    uint8 data[1];
  };
#pragma pack(pop)

  void DoWrite(void* dest, const void* src, uint32 num);

  // Used for reading and writing.
  BufferData* buffer_data_;
  uint32 memory_buffer_size_;
  uint32 data_size_;
  uint32 position_;

  // Used for reading.
  uint32 total_read_;
};

#endif  // CHROME_COMMON_PARTIAL_CIRCULAR_BUFFER_H_
