// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BINDER_TRANSACTION_DATA_FROM_DRIVER_H_
#define BINDER_TRANSACTION_DATA_FROM_DRIVER_H_

#include <unistd.h>
#include <linux/android/binder.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/binder/transaction_data.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace binder {

class CommandStream;

// TransactionData passed by the driver, whose data needs to be freed with
// BC_FREE_BUFFER command.
class TransactionDataFromDriver : public TransactionData {
 public:
  typedef base::Callback<void(const void* ptr)> BufferDeleter;
  explicit TransactionDataFromDriver(const BufferDeleter& buffer_deleter);
  ~TransactionDataFromDriver() override;

  const binder_transaction_data& data() const { return data_; }
  binder_transaction_data* mutable_data() { return &data_; }

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

 private:
  scoped_refptr<base::SingleThreadTaskRunner> delete_task_runner_;
  BufferDeleter buffer_deleter_;
  binder_transaction_data data_;

  DISALLOW_COPY_AND_ASSIGN(TransactionDataFromDriver);
};

}  // namespace binder

#endif  // BINDER_TRANSACTION_DATA_FROM_DRIVER_H_
