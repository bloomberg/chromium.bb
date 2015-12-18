// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_BINDER_WRITABLE_TRANSACTION_DATA_H_
#define CHROMEOS_BINDER_WRITABLE_TRANSACTION_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "chromeos/binder/transaction_data.h"
#include "chromeos/chromeos_export.h"

namespace binder {

// Use this class to construct TransactionData (as parameters and replies) to
// transact with remote objects.
// GetSenderPID() and GetSenderEUID() return 0.
class CHROMEOS_EXPORT WritableTransactionData : public TransactionData {
 public:
  WritableTransactionData();
  ~WritableTransactionData() override;

  // TransactionData override:
  uintptr_t GetCookie() const override;
  uint32 GetCode() const override;
  pid_t GetSenderPID() const override;
  uid_t GetSenderEUID() const override;
  bool IsOneWay() const override;
  bool HasStatus() const override;
  Status GetStatus() const override;
  const void* GetData() const override;
  size_t GetDataSize() const override;
  const uintptr_t* GetObjectOffsets() const override;
  size_t GetNumObjectOffsets() const override;

  // Expands the capacity of the internal buffer.
  void Reserve(size_t n);

  // Sets the transaction code returned by GetCode().
  void SetCode(uint32 code) { code_ = code; }

  // Sets the value returned by IsOneWay().
  void SetIsOneWay(bool is_one_way) { is_one_way_ = is_one_way; }

  // Appends the specified data with appropriate padding.
  void WriteData(const void* data, size_t n);

  // Appends an int32 value.
  void WriteInt32(int32 value);

  // Appends a uint32 value.
  void WriteUint32(uint32 value);

  // Appends an int64 vlaue.
  void WriteInt64(int64 value);

  // Appends a uint64 value.
  void WriteUint64(uint64 value);

  // Appends a float value.
  void WriteFloat(float value);

  // Appends a double value.
  void WriteDouble(double value);
  // TODO(hashimoto): Support more types (i.e. strings, FDs, objects).

 private:
  uint32 code_ = 0;
  bool is_one_way_ = false;
  std::vector<char> data_;
  std::vector<uintptr_t> object_offsets_;

  DISALLOW_COPY_AND_ASSIGN(WritableTransactionData);
};

}  // namespace binder

#endif  // CHROMEOS_BINDER_WRITABLE_TRANSACTION_DATA_H_
