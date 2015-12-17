// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/binder/writable_transaction_data.h"

namespace binder {

WritableTransactionData::WritableTransactionData() {}

WritableTransactionData::~WritableTransactionData() {}

uintptr_t WritableTransactionData::GetCookie() const {
  return 0;
}

uint32 WritableTransactionData::GetCode() const {
  return code_;
}

pid_t WritableTransactionData::GetSenderPID() const {
  return 0;
}

uid_t WritableTransactionData::GetSenderEUID() const {
  return 0;
}

bool WritableTransactionData::IsOneWay() const {
  return is_one_way_;
}

bool WritableTransactionData::HasStatus() const {
  return false;
}

Status WritableTransactionData::GetStatus() const {
  return Status::OK;
}

const void* WritableTransactionData::GetData() const {
  return data_.data();
}

size_t WritableTransactionData::GetDataSize() const {
  return data_.size();
}

const uintptr_t* WritableTransactionData::GetObjectOffsets() const {
  return object_offsets_.data();
}

size_t WritableTransactionData::GetNumObjectOffsets() const {
  return object_offsets_.size();
}

void WritableTransactionData::Reserve(size_t n) {
  data_.reserve(n);
}

void WritableTransactionData::WriteData(const void* data, size_t n) {
  data_.insert(data_.end(), static_cast<const char*>(data),
               static_cast<const char*>(data) + n);
  if (n % 4 != 0) {  // Add padding.
    data_.resize(data_.size() + 4 - (n % 4));
  }
}

void WritableTransactionData::WriteInt32(int32 value) {
  // Binder is not used for inter-device communication, so no endian conversion.
  // The same applies to other Write() methods.
  WriteData(&value, sizeof(value));
}

void WritableTransactionData::WriteUint32(uint32 value) {
  WriteData(&value, sizeof(value));
}

void WritableTransactionData::WriteInt64(int64 value) {
  WriteData(&value, sizeof(value));
}

void WritableTransactionData::WriteUint64(uint64 value) {
  WriteData(&value, sizeof(value));
}

void WritableTransactionData::WriteFloat(float value) {
  WriteData(&value, sizeof(value));
}

void WritableTransactionData::WriteDouble(double value) {
  WriteData(&value, sizeof(value));
}

}  // namespace binder
