// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef MKVREADABLEWRITER_HPP
#define MKVREADABLEWRITER_HPP

#include <stdio.h>

#include "mkvmuxer.hpp"
#include "mkvmuxertypes.hpp"

namespace mkvmuxer {

// Default implementation of the IMkvReadableWriter interface.
class MkvReadableWriter : public IMkvReadableWriter {
 public:
  MkvReadableWriter();
  virtual ~MkvReadableWriter();

  // IMkvReadableWriter interface
  virtual int64 Position() const;
  virtual int32 Position(int64 position);
  virtual bool Seekable() const;
  virtual int32 Write(const void* buffer, uint32 length);
  virtual void ElementStartNotify(uint64 element_id, int64 position);
  virtual int Read(long long position, long length, unsigned char* buffer);
  virtual int Length(long long* total, long long* available);

  // Creates and opens a file for reading and writing. |filename| is the name of
  // the file to open. This function will overwrite the contents of |filename|.
  // If |create_temp_file| is set, the |filename| parameter is ignored and a
  // temporary file is created, which is automatically deleted upon close.
  // Returns true on success.
  bool Open(const char* filename, bool create_temp_file);

  // Closes an opened file.
  void Close();

 private:
  // File handle to output file.
  FILE* file_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(MkvReadableWriter);
};

}  // end namespace mkvmuxer

#endif  // MKVREADABLEWRITER_HPP
