// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/binder/transaction_data_reader.h"

#include "chromeos/binder/transaction_data.h"

namespace binder {

namespace {

// Adds appropriate padding to the given size to make it 4-byte aligned.
size_t AddPadding(size_t n) {
  return (n + 3) & (~3);
}

}  // namespace

TransactionDataReader::TransactionDataReader(const TransactionData& data)
    : data_(data),
      reader_(reinterpret_cast<const char*>(data.GetData()),
              data.GetDataSize()) {}

TransactionDataReader::~TransactionDataReader() {}

bool TransactionDataReader::HasMoreData() const {
  return reader_.HasMoreData();
}

bool TransactionDataReader::ReadData(void* buf, size_t n) {
  return reader_.Read(buf, n) && reader_.Skip(AddPadding(n) - n);
}

bool TransactionDataReader::ReadInt32(int32* value) {
  return ReadData(value, sizeof(*value));
}

bool TransactionDataReader::ReadUint32(uint32* value) {
  return ReadData(value, sizeof(*value));
}

bool TransactionDataReader::ReadInt64(int64* value) {
  return ReadData(value, sizeof(*value));
}

bool TransactionDataReader::ReadUint64(uint64* value) {
  return ReadData(value, sizeof(*value));
}

bool TransactionDataReader::ReadFloat(float* value) {
  return ReadData(value, sizeof(*value));
}

bool TransactionDataReader::ReadDouble(double* value) {
  return ReadData(value, sizeof(*value));
}

}  // namespace binder
