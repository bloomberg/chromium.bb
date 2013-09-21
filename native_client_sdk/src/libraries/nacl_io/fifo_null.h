// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FIFO_NULL_H_
#define LIBRARIES_NACL_IO_FIFO_NULL_H_

#include <stdint.h>
#include <stdlib.h>

namespace nacl_io {

// FIFONull
//
// A null fifo is always ready to read or write, but never actually
// provides or stores data.
class FIFONull : public FIFOInterface {
 public:
  virtual bool IsEmpty() { return false; }
  virtual bool IsFull() { return false; }
  virtual bool Resize(size_t) { return false; }

  virtual uint32_t ReadAvailable() { return 1; }
  virtual uint32_t WriteAvailable() { return 1; }
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FIFO_NULL_H_
