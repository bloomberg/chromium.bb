// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_BINDER_TRANSACTION_DATA_READER_H_
#define CHROMEOS_BINDER_TRANSACTION_DATA_READER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "chromeos/binder/buffer_reader.h"
#include "chromeos/chromeos_export.h"

namespace binder {

class TransactionData;

// Reads contents of a TransactionData.
// Use this class to get parameters of incoming transactions, or to get values
// from transaction replies.
class CHROMEOS_EXPORT TransactionDataReader {
 public:
  explicit TransactionDataReader(const TransactionData& data);
  ~TransactionDataReader();

  // Returns true when there is some data to read.
  bool HasMoreData() const;

  // Reads the specified number of bytes with appropriate padding.
  bool ReadData(void* buf, size_t n);

  // Reads an int32_t value.
  bool ReadInt32(int32_t* value);

  // Reads an uint32_t value.
  bool ReadUint32(uint32_t* value);

  // Reads an int64_t value.
  bool ReadInt64(int64_t* value);

  // Reads an uint64_t value.
  bool ReadUint64(uint64_t* value);

  // Reads a float value.
  bool ReadFloat(float* value);

  // Reads a double value.
  bool ReadDouble(double* value);
  // TODO(hashimoto): Support more types (i.e. strings, FDs, objects).

 private:
  const TransactionData& data_;
  BufferReader reader_;

  DISALLOW_COPY_AND_ASSIGN(TransactionDataReader);
};

}  // namespace binder

#endif  // CHROMEOS_BINDER_TRANSACTION_DATA_READER_H_
